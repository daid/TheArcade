#include "performanceTest.h"

#include <sp2/engine.h>
#include <sp2/scene/node.h>
#include <sp2/graphics/meshdata.h>
#include <sp2/graphics/textureManager.h>
#include <sp2/random.h>
#include <sp2/collision/2d/circle.h>
#include <sp2/scene/camera.h>


PerformanceTestScene::PerformanceTestScene()
: sp::Scene("performance_test")
{
    mesh = sp::MeshData::createQuad(sp::Vector2f(1, 1));

    sp::Camera* camera = new sp::Camera(getRoot());
    camera->setOrtographic(100.0);
    setDefaultCamera(camera);
    
    disable();
}
    
void PerformanceTestScene::onUpdate(float delta)
{
    if (ignore_next)
    {
        ignore_next = false;
        return;
    }
    if (measure_delay > 0)
    {
        measure_delay -= delta;
        return;
    }
    update_deltas.push_back(delta);
    
    if (update_deltas.size() == 10)
    {
        ignore_next = true;

        std::sort(update_deltas.begin(), update_deltas.end());
        float average = 0.0;
        int average_count = 0;
        for(unsigned int n=update_deltas.size()/3;n<=update_deltas.size()*2/3; n++)
        {
            average += update_deltas[n];
            average_count++;
        }
        float fps = 1.0 / (average / average_count);
        result_data.emplace_back(node_count, fps);
        update_deltas.clear();
        
        if (fps > 59.0)
            good_fps_node_count = node_count;
        if (fps < 30.0)
        {
            for(auto it : result_data)
            {
                LOG(Info, it.first, it.second);
            }
            if (result_data.size() >= 30 || ((state == State::Gravity || state == State::GravityRender) && result_data.size() >= 10))
            {
                switch(state)
                {
                case State::NoRender:           result_text += "NoRender        "; break;
                case State::Render:             result_text += "Render          "; break;
                case State::Collision:          result_text += "Collision       "; break;
                case State::CollisionRender:    result_text += "CollisionRender "; break;
                case State::Gravity:            result_text += "Gravity         "; break;
                case State::GravityRender:      result_text += "GravityRender   "; break;
                case State::Finished: break;
                }
                result_text += sp::string(good_fps_node_count) + " " + sp::string(result_data.back().first);
                result_text += "\n";

                state = State(int(state) + 1);

                if (state == State::Finished)
                {
                    disable();
                    sp::Scene::get("MAIN")->enable();
                    LOG(Info, result_text);
                    if (finish_function)
                        finish_function(result_text);
                    FILE* f = fopen("/tmp/performance.test", "wt");
                    if (f)
                    {
                        fwrite(result_text.c_str(), result_text.length(), 1, f);
                        fclose(f);
                    }
                    return;
                }
                
                node_create_step_count = 1000;
                good_fps_node_count = 0;
            }
            result_data.clear();

            node_create_step_count /= 2;
            LOG(Info, "Erasing nodes");
            eraseNodes();
            node_count = 0;
            LOG(Info, "Next round: start:", good_fps_node_count, "steps:", node_create_step_count);
            good_fps_node_count -= node_create_step_count;
            while(node_count < good_fps_node_count)
                createNode();
        }
        else
        {
            for(int n=0; n<node_create_step_count; n++)
                createNode();
        }
        if (state == State::Gravity || state == State::GravityRender)
            measure_delay = 2.0;
    }
}
    
void PerformanceTestScene::onFixedUpdate()
{
    if (state == State::Gravity || state == State::GravityRender)
    {
        for(sp::Node* node : getRoot()->getChildren())
        {
            node->setLinearVelocity(-node->getPosition2D());
        }
    }
}

void PerformanceTestScene::onEnable()
{
    state = State::NoRender;
    result_text = "";
}

void PerformanceTestScene::createNode()
{
    sp::Node* node = new sp::Node(getRoot());
    if (state == State::Render || state == State::CollisionRender || state == State::GravityRender)
    {
        node->render_data.type = sp::RenderData::Type::Normal;
        node->render_data.shader = sp::Shader::get("internal:basic.shader");
        node->render_data.mesh = mesh;
        node->render_data.texture = sp::texture_manager.get("gui/theme/pixel.png");
    }
    node->setPosition(sp::Vector2d(sp::random(-100, 100), sp::random(-100, 100)));
    if (state == State::Collision || state == State::CollisionRender || state == State::Gravity || state == State::GravityRender)
        node->setCollisionShape(sp::collision::Circle2D(1.0));
    node_count++;
}

void PerformanceTestScene::eraseNodes()
{
    for(sp::Node* node : getRoot()->getChildren())
    {
        sp::P<sp::Camera> camera = sp::P<sp::Node>(node);
        if (camera)
            continue;
        delete node;
    }
}
