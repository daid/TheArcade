#include <sp2/engine.h>
#include <sp2/window.h>
#include <sp2/logging.h>
#include <sp2/random.h>
#include <sp2/io/keybinding.h>
#include <sp2/io/subprocess.h>
#include <sp2/io/directoryResourceProvider.h>
#include <sp2/io/keyValueTreeLoader.h>
#include <sp2/graphics/gui/scene.h>
#include <sp2/graphics/gui/theme.h>
#include <sp2/graphics/gui/loader.h>
#include <sp2/graphics/gui/widget/button.h>
#include <sp2/graphics/gui/widget/slider.h>
#include <sp2/graphics/scene/graphicslayer.h>
#include <sp2/graphics/scene/basicnoderenderpass.h>
#include <sp2/graphics/scene/collisionrenderpass.h>
#include <sp2/graphics/meshdata.h>
#include <sp2/graphics/textureManager.h>
#include <sp2/graphics/opengl.h>
#include <sp2/scene/scene.h>
#include <sp2/scene/camera.h>
#include <sp2/collision/2d/box.h>
#include <sp2/collision/2d/circle.h>

#include <sys/stat.h>

#ifdef __WIN32__
#include <windows.h>
#define GIT "C:/Program Files/Git/mingw64/bin/git.exe"
#else
#include <X11/Xlib.h>
#define GIT "git"
#endif//__WIN32__

sp::P<sp::Scene> scene;
sp::P<sp::Camera> camera;
sp::P<sp::SceneGraphicsLayer> scene_layer;

sp::io::Keybinding up_key("UP", "up");
sp::io::Keybinding down_key("DOWN", "down");
sp::io::Keybinding go_key("GO", "space");

bool isAnyKeyPressed()
{
#ifdef __WIN32__
    uint8_t keys[256];
    GetKeyboardState(keys);
    for(int n=0; n<256;n++)
        if (keys[n] & 0x80)
            return true;
#else
    Display* display = XOpenDisplay(nullptr);
    char keys[32];
    XQueryKeymap(display, keys);
    XCloseDisplay(display);
    for(int n=0; n<32; n++)
        if (keys[n])
            return true;
#endif
    return false;
}

bool isDirectory(sp::string directory)
{
    struct stat s;
    if (stat(directory.c_str(), &s))
        return false;
    return S_ISDIR(s.st_mode);
}

class GameNode : public sp::Node
{
public:
    GameNode(sp::P<sp::Node> parent, sp::string name)
    : sp::Node(parent), name(name)
    {
        render_data.type = sp::RenderData::Type::Normal;
        render_data.shader = sp::Shader::get("internal:basic.shader");
        render_data.mesh = sp::MeshData::createDoubleSidedQuad(sp::Vector2f(4.0/3.0, 1));
        render_data.texture = sp::texture_manager.get("loading.png");
    }
    
    void run()
    {
        if (state != State::Ready)
            return;
        LOG(Info, "Running:", exec, "@", name);

        sp::io::Subprocess process({"_build/" + exec}, name);
        float timeout = inactivity_timeout;
        while(process.isRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (isAnyKeyPressed())
                timeout = inactivity_timeout;
            else
                timeout -= 0.1;
        }
    }

    void doASyncLoad()
    {
        state = State::Loading;
        LOG(Info, name, ": Loading");
        if (exec == "" || git == "")
        {
            LOG(Error, name, ": No exec or git info");
            state = State::Error;
            return;
        }
        if (depends_repo != "")
            if (!updateGit(depends_repo, depends_path))
                return;
        if (!updateGit(git, name))
            return;
        sp::string build_path = name + "/_build";
#ifdef __WIN32__
        mkdir(build_path.c_str());
#else
        mkdir(build_path.c_str(), 0777);
#endif
        for(sp::string command : build_commands)
        {
            LOG(Info, name, ": Running build command:", command, "at", build_path);
            sp::io::Subprocess build_process(command.strip().split(" "), build_path);
            if (build_process.wait() != 0)
            {
                LOG(Error, name, ": Failed to build:", command);
                state = State::Error;
                return;
            }
        }
        
        state = State::Ready;
        LOG(Info, name, ": Ready");
    }
    
    bool updateGit(sp::string repo, sp::string target)
    {
        if (!isDirectory(target))
        {
            LOG(Info, name, ": Running git clone for", target);
            sp::io::Subprocess git_process({GIT, "clone", repo, target});
            if (git_process.wait() != 0)
            {
                LOG(Error, name, ": Failed to clone repository", repo);
                state = State::Error;
                return false;
            }
        }
        else
        {
            LOG(Info, name, ": Running git pull for", target);
            sp::io::Subprocess git_process({GIT, "pull"}, target);
            if (git_process.wait() != 0)
            {
                LOG(Error, name, ": Failed to pull repository", repo);
                state = State::Error;
                return false;
            }
        }
        return true;
    }
    
    enum class State
    {
        Waiting,
        Loading,
        Ready,
        Error
    };
    volatile State state = State::Waiting;

    static constexpr float inactivity_timeout = 60 * 5;
    sp::string name;
    sp::string exec;
    sp::string git;
    sp::string depends_repo;
    sp::string depends_path;
    std::vector<sp::string> build_commands;
};

class Spinner : public sp::Node
{
public:
    Spinner(sp::P<Node> parent)
    : sp::Node(parent)
    {
        setRotation(sp::Quaterniond::fromAxisAngle(sp::Vector3d(0,1,0), 15));

        item_count = 0;
        target_rotation = 0.0;
        rotation = 0;

        for(auto& it : sp::io::KeyValueTreeLoader::load("games.txt")->getFlattenNodesByIds())
        {
            sp::P<GameNode> game = add(it.first);
            game->exec = it.second["exec"];
            game->git = it.second["git"];
            std::vector<sp::string> depends = it.second["depends"].split(" ");
            if (depends.size() > 1)
            {
                game->depends_path = depends[0];
                game->depends_repo = depends[1];
            }
            game->build_commands = it.second["build"].split("\n");
        }
    }

    virtual void onFixedUpdate() override
    {
        rotation = rotation * 0.9 + target_rotation * 0.1;
        setRotation(sp::Quaterniond::fromAxisAngle(sp::Vector3d(0,1,0), 15) * sp::Quaterniond::fromAxisAngle(sp::Vector3d(1,0,0), rotation));
    }
    
    virtual void onUpdate(float delta) override
    {
        float angle_per_item = 360.0 / float(item_count);
        if (std::abs(rotation - target_rotation) < angle_per_item / 3.0)
        {
            if (up_key.getDown())
                target_rotation -= angle_per_item;
            if (down_key.getDown())
                target_rotation += angle_per_item;
            if (go_key.getDown())
            {
                double item_offset = -target_rotation / angle_per_item;
                while(item_offset < 0)
                    item_offset += item_count;
                int index = item_offset + 0.5;
                while(index >= item_count)
                    index -= item_count;
                for(GameNode* game : games)
                {
                    if (index == 0)
                        game->run();
                    index--;
                }
            }
        }
    }
    
    sp::P<GameNode> add(sp::string name)
    {
        sp::P<sp::Node> rotation_node = new sp::Node(this);
        sp::P<sp::Node> offset_node = new sp::Node(rotation_node);
        GameNode* game = new GameNode(offset_node, name);

        games.push_back(game);
        
        update();
        return game;
    }
    
    void doASyncLoad()
    {
        for(GameNode* game : games)
        {
            game->doASyncLoad();
        }
    }
    
private:
    int item_count;
    float rotation;
    float target_rotation;
    
    std::vector<GameNode*> games; //using an std::vector as sp::PList is not thread safe at all, even on a static list.
    
    void update()
    {
        item_count = getChildren().size();
        float distance = float(item_count) * 1.2f / (2.0f * sp::pi);
        float angle_per_item = 360.0 / float(item_count);
        int index = 0;
        for(auto node : getChildren())
        {
            node->setRotation(sp::Quaterniond::fromAxisAngle(sp::Vector3d(1,0,0), index * angle_per_item));
            for(auto n : node->getChildren())
                n->setPosition(sp::Vector3d(0, 0, distance));
            index++;
        }
        
        setPosition(sp::Vector3d(-distance * 0.25, 0, -distance * 0.9 - 2));
    }
};


int main(int argc, char** argv)
{
    sp::P<sp::Engine> engine = new sp::Engine();
    //Create resource providers, so we can load things.
    new sp::io::DirectoryResourceProvider("resources");
    
    //Load our ui theme.
    sp::gui::Theme::loadTheme("default", "gui/theme/basic.theme.txt");
    
    //Create a window to render on, and our engine.
    sp::P<sp::Window> window = new sp::Window(4.0/3.0);
    
    scene = new sp::Scene("MAIN");
    camera = new sp::Camera(scene->getRoot());
    camera->setPerspective();
    camera->setPosition(sp::Vector3d(0, 0, 0));

    Spinner* spinner_node = new Spinner(scene->getRoot());
    
    {
        new sp::gui::Scene(sp::Vector2d(1280, 800));

        scene_layer = new sp::SceneGraphicsLayer(10);
        scene_layer->addRenderPass(new sp::BasicNodeRenderPass(camera));
        window->addLayer(scene_layer);
    }
    
    std::thread async_load([spinner_node]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        spinner_node->doASyncLoad();
    });
    
    engine->run();

    async_load.join();
    
    return 0;
}
