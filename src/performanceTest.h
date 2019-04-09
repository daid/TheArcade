#ifndef PERFORMANCE_TEST_H
#define PERFORMANCE_TEST_H

#include <sp2/scene/scene.h>


class PerformanceTestScene : public sp::Scene
{
public:
    PerformanceTestScene();

    virtual void onUpdate(float delta) override;
    virtual void onFixedUpdate() override;
    virtual void onEnable() override;
    
    std::function<void(sp::string)> finish_function;
private:
    void createNode();
    void eraseNodes();

private:
    enum class State
    {
        NoRender,
        RenderOnly,
        Collision,
        Gravity,
        Finished
    } state;
    
    std::shared_ptr<sp::MeshData> mesh;
    float measure_delay = 0;
    int node_create_step_count = 1000;
    int node_count = 0;
    int good_fps_node_count = 0;
    std::vector<float> update_deltas;
    std::vector<std::pair<int, float>> result_data;
    bool ignore_next = true;
    
    sp::string result_text;
};

#endif//PERFORMANCE_TEST_H
