
#include "../model/Model.hpp"
#include "../matek/Kif.hpp"

using namespace Matek::Analizis;

struct Const {
    float delta_t = 0.03f;
    float phi = 15.0f;
    float rho = 15.0f;
    float alpha = 6.0f;
    float E = 0.8f*alpha;
    float beta = 10.0f;
    //szigmák értékei függvények
    float gamma = 4.0f;
    float nu = 0.2f;
    float delta = 0.7f;
};

struct Particle {
    glm::vec3 p;
    glm::vec3 v;
    float sigma;
    bool is_control;
};

template<size_t L>
struct Surface {
    Kif F;
    std::function<glm::vec<L, float>(float)> q;

    std::vector<Particle> floaters;
    std::vector<Particle> controls;
};

class ImplicitSurface : public Model {
    Surface<4> surface;

public:

    ImplicitSurface() {

        float p = 2.0f;
        surface.F = (x ^ 3.0f) + 4.0_k * (&p);

        for (int i = 0; i < 10; i++) {
            p++;
            std::cout << surface.F.at({0, 1, 1}) << " ";
            std::cout << surface.F.derrive(&p).at({1, 1, 1}) << "\n";
        }


    }

protected:
    void render(const Camera &camera) const override {
        glDrawArrays(GL_POINTS, 0, vertices.size());
    }
};



