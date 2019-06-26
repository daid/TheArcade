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
#include <sp2/io/filesystem.h>

#include "unfocusedKeyInfo.h"
#include "performanceTest.h"
#include "cameraCaptureTexture.h"

#define GIT "git"

sp::P<sp::Scene> scene;
sp::P<sp::Camera> camera;
sp::P<sp::SceneGraphicsLayer> scene_layer;

sp::io::Keybinding up_key("UP", "up");
sp::io::Keybinding down_key("DOWN", "down");
sp::io::Keybinding left_key("LEFT", "left");
sp::io::Keybinding right_key("RIGHT", "right");
sp::io::Keybinding go_key("GO", "space");

sp::io::Keybinding switch_beta_key("SWITCH_BETA", "b");

sp::io::Keybinding secret_key("SECRET", "1");
sp::io::Keybinding camera_key("CAMERA", "2");

CameraCaptureTexture* camera_capture_texture;
sp::P<sp::Node> camera_display_node;


class GameNode : public sp::Node
{
public:
    GameNode(sp::P<sp::Node> parent, sp::string name)
    : sp::Node(parent), name(name)
    {
        render_data.type = sp::RenderData::Type::None;
        render_data.shader = sp::Shader::get("internal:basic.shader");
        render_data.mesh = sp::MeshData::createDoubleSidedQuad(sp::Vector2f(4.0/3.0, 1));
        render_data.texture = sp::texture_manager.get("loading.png");
    }
    
    virtual void onUpdate(float delta)
    {
        if (prev_update_state == state)
            return;
        prev_update_state = state;
        switch(state)
        {
        case State::Waiting:
        case State::Loading:
            render_data.texture = sp::texture_manager.get("loading.png");
            break;
        case State::Ready:
            render_data.texture = sp::texture_manager.get(name + "/preview.png");
            break;
        case State::Error:
            render_data.texture = sp::texture_manager.get("error.png");
            break;
        }
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
            if (timeout <= 0 || isExitKeyPressed())
            {
                timeout = inactivity_timeout;
                process.kill(true);
            }
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
        sp::io::makeDirectory(build_path);
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
        if (!sp::io::isDirectory(target))
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
    State prev_update_state = State::Waiting;
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
    Spinner(sp::P<Node> parent, sp::P<sp::gui::Widget> gui, sp::string resource_name)
    : sp::Node(parent), gui(gui)
    {
        setRotation(sp::Quaterniond::fromAxisAngle(sp::Vector3d(0,1,0), 15));

        target_rotation = 0.0;
        rotation = 0;

        for(auto& it : sp::io::KeyValueTreeLoader::load(resource_name)->getFlattenNodesByIds())
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
    
    void setActive(bool a)
    {
        active = a;
        for(auto game_node : games)
        {
            if (active)
                game_node->render_data.type = sp::RenderData::Type::Normal;
            else
                game_node->render_data.type = sp::RenderData::Type::None;
        }
        if (active)
            updateCurrentGame();
    }
    
    bool isActive()
    {
        return active;
    }

    virtual void onFixedUpdate() override
    {
        if (!active) return;
        rotation = rotation * 0.9 + target_rotation * 0.1;
        setRotation(sp::Quaterniond::fromAxisAngle(sp::Vector3d(0,1,0), 15) * sp::Quaterniond::fromAxisAngle(sp::Vector3d(1,0,0), rotation));
    }
    
    virtual void onUpdate(float delta) override
    {
        if (!active) return;
        float angle_per_item = 360.0 / float(games.size());
        if (std::abs(rotation - target_rotation) < angle_per_item / 3.0)
        {
            if (up_key.getDown())
            {
                target_rotation -= angle_per_item;
                updateCurrentGame();
            }
            if (down_key.getDown())
            {
                target_rotation += angle_per_item;
                updateCurrentGame();
            }
            if (go_key.getDown())
            {
                if (camera_display_node)
                {
                    camera_display_node.destroy();
                    camera_capture_texture->close();
                }
                current_game->run();
            }
            if (camera_key.get() && secret_key.getDown())
            {
                if (camera_display_node)
                {
                    camera_display_node.destroy();
                    camera_capture_texture->close();
                }

                gui->getWidgetWithID("NAME")->setAttribute("caption", "TEST...");
                sp::Scene::get("performance_test")->enable();
                (sp::P<PerformanceTestScene>(sp::Scene::get("performance_test")))->finish_function = [this](sp::string result)
                {
                    gui->getWidgetWithID("INFO")->setAttribute("caption", result);
                };
                getScene()->disable();
            }
            
            if (camera_key.getDown())
            {
                if (camera_display_node)
                {
                    camera_display_node.destroy();
                    camera_capture_texture->close();
                }
                else
                {
                    if (!camera_capture_texture)
                        camera_capture_texture = new CameraCaptureTexture();
                    camera_capture_texture->open(0);
                    
                    camera_display_node = new sp::Node(getParent());
                    camera_display_node->setPosition(sp::Vector3d(1, 0, -2));
                    camera_display_node->setRotation(-90);
                    camera_display_node->render_data.type = sp::RenderData::Type::Normal;
                    camera_display_node->render_data.shader = sp::Shader::get("internal:basic.shader");
                    camera_display_node->render_data.mesh = sp::MeshData::createDoubleSidedQuad(sp::Vector2f(4.0/3.0, 1));
                    camera_display_node->render_data.texture = camera_capture_texture;
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
        updateCurrentGame();
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
    bool active = false;
    
    float rotation;
    float target_rotation;
    
    std::vector<GameNode*> games; //using an std::vector as sp::PList is not thread safe at all, even on a static list.
    GameNode* current_game = nullptr;
    
    sp::P<sp::gui::Widget> gui;
    
    void update()
    {
        float distance = float(games.size()) * 1.2f / (2.0f * sp::pi);
        float angle_per_item = 360.0 / float(games.size());
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
    
    void updateCurrentGame()
    {
        if (current_game)
            current_game->render_data.order = 0;

        float angle_per_item = 360.0 / float(games.size());
        double item_offset = -target_rotation / angle_per_item;
        while(item_offset < 0)
            item_offset += games.size();
        int index = item_offset + 0.5;
        while(index >= int(games.size()))
            index -= games.size();
        for(GameNode* game : games)
        {
            if (index == 0)
                current_game = game;
            index--;
        }
        
        gui->getWidgetWithID("NAME")->setAttribute("caption", current_game->name);
        gui->getWidgetWithID("INFO")->setAttribute("caption", current_game->git);
        current_game->render_data.order = 1;
    }
};

class BetaSwitcher : public sp::Node
{
public:
    BetaSwitcher(sp::P<sp::Node> parent, sp::P<Spinner> normal, sp::P<Spinner> beta, sp::P<sp::gui::Widget> gui)
    : sp::Node(parent), normal(normal), beta(beta), gui(gui)
    {
    }

    virtual void onFixedUpdate()
    {
        if (timeout > 0)
        {
            timeout--;
            if (!timeout)
                switchActive();
        }
        if (switch_beta_key.getDown())
            switchActive();
        if (left_key.getDown() && !normal->isActive())
            switchActive();
        if (right_key.getDown() && normal->isActive())
            switchActive();
    }
    
    void switchActive()
    {
        bool normal_active = !normal->isActive();
        normal->setActive(normal_active);
        beta->setActive(!normal_active);
        timeout = 0;
        if (!normal_active)
            timeout = 60 * 60 * 5;
        gui->getWidgetWithID("BETA")->setVisible(!normal_active);
    }
    
private:
    int timeout;
    sp::P<Spinner> normal;
    sp::P<Spinner> beta;
    sp::P<sp::gui::Widget> gui;
};


int main(int argc, char** argv)
{
    sp::P<sp::Engine> engine = new sp::Engine();
    //Create resource providers, so we can load things.
    new sp::io::DirectoryResourceProvider("resources");
    new sp::io::DirectoryResourceProvider(".");
    
    //Load our ui theme.
    sp::gui::Theme::loadTheme("default", "gui/theme/basic.theme.txt");
    
    //Create a window to render on, and our engine.
    sp::P<sp::Window> window = new sp::Window(4.0/3.0);
#ifndef DEBUG
    window->setFullScreen(true);
    window->hideCursor();
#endif
    
    new sp::gui::Scene(sp::Vector2d(400, 300));
    
    scene = new sp::Scene("MAIN");
    camera = new sp::Camera(scene->getRoot());
    camera->setPerspective();
    camera->setPosition(sp::Vector3d(0, 0, 0));
    scene->setDefaultCamera(camera);

    sp::P<sp::gui::Widget> gui = sp::gui::Loader::load("main.gui", "MAIN");
    Spinner* spinner_node = new Spinner(scene->getRoot(), gui, "games.txt");
    Spinner* spinner_node_beta = new Spinner(scene->getRoot(), gui, "beta_games.txt");
    new BetaSwitcher(scene->getRoot(), spinner_node, spinner_node_beta, gui);
    spinner_node->setActive(true);
    
    scene_layer = new sp::SceneGraphicsLayer(1);
    scene_layer->addRenderPass(new sp::BasicNodeRenderPass());
    window->addLayer(scene_layer);
    
    std::thread async_load([spinner_node, spinner_node_beta]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        spinner_node->doASyncLoad();
        spinner_node_beta->doASyncLoad();
    });
    
    new PerformanceTestScene();
    
    engine->run();

    async_load.join();
    
    return 0;
}
