#pragma once
#include "engine.h"

namespace engine {

class UCIEngine {
public:
    void loop();

private:
    Engine engine_;
};

}  // namespace engine
