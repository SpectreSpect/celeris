#include "reeds_shepp.h"


namespace {
    constexpr float PI     = 3.14159265358979323846f;
    constexpr float HALFPI = 1.57079632679489661923f;
    constexpr float EPS    = 1e-6f;

    static inline float clamp1(float x) {
        return std::max(-1.0f, std::min(1.0f, x));
    }
}


// Return glm::vec3(x, y, theta)
glm::vec3 ReedsShepp::change_of_basis_r(glm::vec3& p1, glm::vec3& p2, float min_radius) {
    float dx = (p2.x - p1.x) / min_radius;
    float dy = (p2.y - p1.y) / min_radius;

    float theta_1 = p1.z;
    float theta_2 = p2.z;

    float nx = dx * cos(theta_1) + dy * sin(theta_1);
    float ny = -dx * sin(theta_1) + dy * cos(theta_1);
    float n_theta = theta_2 - theta_1;

    return glm::vec3(nx, ny, n_theta);
}

NonholonomicPos ReedsShepp::change_of_basis_r(NonholonomicPos& p1, NonholonomicPos& p2, float min_radius) {
    float dx = (p2.pos.x - p1.pos.x) / min_radius;
    float dz = (p2.pos.z - p1.pos.z) / min_radius;

    NonholonomicPos new_pos;

    new_pos.pos.x = dx * cos(p1.theta) + dz * sin(p1.theta);
    new_pos.pos.z = -dx * sin(p1.theta) + dz * cos(p1.theta);
    new_pos.theta = p2.theta - p1.theta;

    return new_pos;
}

// Returns glm::vec2(radius, angle)
glm::vec2 ReedsShepp::cart_to_polar(glm::vec2 pos) {
    float r = sqrt(pos.x*pos.x + pos.y*pos.y);
    
    if (r == 0)
        return glm::vec2(0, 0);
    
    float theta = atan2(pos.y, pos.x);

    return glm::vec2(r, theta);
}

float ReedsShepp::wrap_pi(float theta) {
    constexpr double PI = 3.1415926535897932384626433832795;
    constexpr double TWO_PI = 2.0 * PI;

    // bring into (-2pi, 2pi) range first
    theta = std::fmod(theta, TWO_PI);

    // wrap to [-pi, pi)
    if (theta < -PI) theta += TWO_PI;
    if (theta >= PI) theta -= TWO_PI;

    return theta;
}

std::vector<NonholonomicPos> ReedsShepp::discretize_path(NonholonomicPos start_pos, std::vector<NonholonomicPathElement> path, int points_per_circlar_element, float min_radius) {
    std::vector<NonholonomicPos> output_path;

    output_path.push_back(start_pos);

    NonholonomicPos last_point = output_path[0];
    for (int i = 0; i < path.size(); i++) {
        float gear = path[i].gear == Gear::FORWARD ? 1 : -1;

        if (path[i].steering == Steering::STRAIGHT) {
            glm::vec3 dir = glm::vec3(cos(last_point.theta), 0, sin(last_point.theta));
            last_point.pos += dir * (path[i].dist * min_radius) * gear;

            last_point.dir = gear;
            output_path.push_back(last_point);
        }
        else {
            float steering = path[i].steering == Steering::LEFT ? -1 : 1;
            float ds = ((path[i].dist * min_radius) / (float)points_per_circlar_element) * gear;
            float k = steering / min_radius;
            
            for (int step = 0; step < points_per_circlar_element; step++) {
                const float theta0 = last_point.theta;
                const float dtheta = k * ds; // reverse flips rotation
                
                last_point.pos.x += (std::sin(theta0 + dtheta) - std::sin(theta0)) / k;
                last_point.pos.z += (-std::cos(theta0 + dtheta) + std::cos(theta0)) / k;
                last_point.theta = theta0 + dtheta;
                last_point.dir = gear;
                
                // glm::vec3(cos(stride * steering), 0, sin(stride * steering)) * gear;  
                
                output_path.push_back(last_point);
            }
        }
    }

    return output_path;
}

std::vector<NonholonomicPathElement> ReedsShepp::timeflip(std::vector<NonholonomicPathElement> path) {
    for (int i = 0; i < path.size(); i++)
        path[i].reverse_gear();
    
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::reflect(std::vector<NonholonomicPathElement> path) {
    for (int i = 0; i < path.size(); i++)
        path[i].reverse_steering();
    
    return path;
}

void ReedsShepp::remove_zero_segments(std::vector<NonholonomicPathElement>& path, float eps) {
    path.erase(std::remove_if(path.begin(), path.end(),
                        [eps](const NonholonomicPathElement& e) {
                            return std::abs(e.dist) <= eps;
                        }),
        path.end());
}

float ReedsShepp::get_length(std::vector<NonholonomicPathElement>& path) {
    float length = 0;
    for (int i = 0; i < path.size(); i++)
        length += path[i].dist;
    return length;
}

std::vector<std::vector<NonholonomicPathElement>> ReedsShepp::get_all_paths(NonholonomicPos start, NonholonomicPos end, float min_radius) {
    start.pos.z = -start.pos.z;
    start.theta = wrap_pi(-start.theta);

    end.pos.z = -end.pos.z;
    end.theta = wrap_pi(-end.theta);

    
    using PathFn = std::vector<NonholonomicPathElement>(*)(float, float, float);

    static constexpr std::array<PathFn, 12> path_fns = {
        &ReedsShepp::path1,  &ReedsShepp::path2,  &ReedsShepp::path3,  &ReedsShepp::path4,
        &ReedsShepp::path5,  &ReedsShepp::path6,  &ReedsShepp::path7,  &ReedsShepp::path8,
        &ReedsShepp::path9,  &ReedsShepp::path10, &ReedsShepp::path11, &ReedsShepp::path12
    };

    std::vector<std::vector<NonholonomicPathElement>> paths;
    paths.reserve(path_fns.size() * 4);

    NonholonomicPos local = change_of_basis_r(const_cast<NonholonomicPos&>(start), const_cast<NonholonomicPos&>(end), min_radius);

    float x = local.pos.x;
    float y = local.pos.z;
    float phi = wrap_pi(local.theta);

    for (auto fn : path_fns) {
        {
            auto p = fn(x, y, phi);
            remove_zero_segments(p);
            if (!p.empty()) paths.push_back(p);
        }

        {
            auto p = fn(-x, y, wrap_pi(-phi));
            p = timeflip(std::move(p));
            remove_zero_segments(p);
            if (!p.empty()) paths.push_back(std::move(p));
        }

        {
            auto p = fn(x, -y, wrap_pi(-phi));
            p = reflect(std::move(p));
            remove_zero_segments(p);
            if (!p.empty()) paths.push_back(std::move(p));
        }

        {
            auto p = fn(-x, -y, phi);
            p = timeflip(std::move(p));
            p = reflect(std::move(p));
            remove_zero_segments(p);
            if (!p.empty()) paths.push_back(std::move(p));
        }
    }

    return paths;
}

std::vector<NonholonomicPathElement> ReedsShepp::get_optimal_path(NonholonomicPos start, NonholonomicPos end, float min_radius) {
    std::vector<std::vector<NonholonomicPathElement>> paths = get_all_paths(start, end, min_radius);
    
    if (paths.empty())
        return {};
    
    float min_length = get_length(paths[0]);
    int min_id = 0;

    if (paths.size() == 1)
        return paths[min_id];

    for (int i = 1; i < paths.size(); i++) {
        float length = get_length(paths[i]);

        if (length < min_length) {
            min_length = length;
            min_id = i;
        }
    }

    return paths[min_id];
}

std::vector<NonholonomicPos> ReedsShepp::get_optimal_path_discretized(
                                                                      NonholonomicPos start, NonholonomicPos end, 
                                                                      int points_per_circlar_element, float min_radius) {
    std::vector<NonholonomicPathElement> reeds_shepp_test_path = get_optimal_path(start, end, min_radius);
    std::vector<NonholonomicPos> discretized_path = discretize_path(start, reeds_shepp_test_path, points_per_circlar_element, min_radius);
    return discretized_path;
    // std::vector<LineInstance> reeds_shepp_test_line_instances;
    // if (discretized_path.size() >= 2)
    // for (int i = 0; i < discretized_path.size() - 1; i++) {
    //     LineInstance line_instance;
    //     line_instance.p0 = discretized_path[i].pos;
    //     line_instance.p1 = discretized_path[i+1].pos;

    //     reeds_shepp_test_line_instances.push_back(line_instance);

    //     reeds_shepp_test_path_lines->set_lines(reeds_shepp_test_line_instances);
    // }
}

// CSC path
std::vector<NonholonomicPathElement> ReedsShepp::path1(float x, float y, float phi) {
    std::vector<NonholonomicPathElement> path;

    glm::vec2 polar_pos = cart_to_polar(glm::vec2(x - sin(phi), y - 1 + cos(phi)));
    float u = polar_pos.x;
    float t = polar_pos.y;
    float v = wrap_pi(phi - t);

    path.push_back(NonholonomicPathElement(t, Steering::LEFT, Gear::FORWARD));
    path.push_back(NonholonomicPathElement(u, Steering::STRAIGHT, Gear::FORWARD));
    path.push_back(NonholonomicPathElement(v, Steering::LEFT, Gear::FORWARD));

    return path;
}


std::vector<NonholonomicPathElement> ReedsShepp::path2(float x, float y, float phi) {
    // Formula 8.2: CSC (opposite turns) -> L S R
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    glm::vec2 polar = cart_to_polar(glm::vec2(x + std::sin(phi), y - 1.0f - std::cos(phi)));
    float rho = polar.x;
    float t1  = polar.y;

    if (rho * rho >= 4.0f) {
        float u = std::sqrt(rho * rho - 4.0f);
        float t = wrap_pi(t1 + std::atan2(2.0f, u));
        float v = wrap_pi(t - phi);

        path.push_back(NonholonomicPathElement(t, Steering::LEFT,    Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::STRAIGHT,Gear::FORWARD));
        path.push_back(NonholonomicPathElement(v, Steering::RIGHT,   Gear::FORWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path3(float x, float y, float phi) {
    // Formula 8.3: C|C|C
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x - std::sin(phi);
    float eta = y - 1.0f + std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho <= 4.0f) {
        float A = std::acos(clamp1(rho / 4.0f));
        float t = wrap_pi(theta + HALFPI + A);
        float u = wrap_pi(PI - 2.0f * A);
        float v = wrap_pi(phi - t - u);

        path.push_back(NonholonomicPathElement(t, Steering::LEFT,  Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::RIGHT, Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v, Steering::LEFT,  Gear::FORWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path4(float x, float y, float phi) {
    // Formula 8.4 (1): C|CC
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x - std::sin(phi);
    float eta = y - 1.0f + std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho <= 4.0f) {
        float A = std::acos(clamp1(rho / 4.0f));
        float t = wrap_pi(theta + HALFPI + A);
        float u = wrap_pi(PI - 2.0f * A);
        float v = wrap_pi(t + u - phi);

        path.push_back(NonholonomicPathElement(t, Steering::LEFT,  Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::RIGHT, Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v, Steering::LEFT,  Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path5(float x, float y, float phi) {
    // Formula 8.4 (2): CC|C
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x - std::sin(phi);
    float eta = y - 1.0f + std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho <= 4.0f) {
        float u = std::acos(clamp1(1.0f - (rho * rho) / 8.0f));

        float A;
        if (rho < EPS) {
            // limit rho->0 gives 2*sin(u)/rho -> 1
            A = HALFPI;
        } else {
            A = std::asin(clamp1((2.0f * std::sin(u)) / rho));
        }

        float t = wrap_pi(theta + HALFPI - A);
        float v = wrap_pi(t - u - phi);

        path.push_back(NonholonomicPathElement(t, Steering::LEFT,  Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::RIGHT, Gear::FORWARD));
        path.push_back(NonholonomicPathElement(v, Steering::LEFT,  Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path6(float x, float y, float phi) {
    // Formula 8.7: CCu|CuC
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x + std::sin(phi);
    float eta = y - 1.0f - std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho <= 4.0f) {
        float A, t, u, v;

        if (rho <= 2.0f) {
            A = std::acos(clamp1((rho + 2.0f) / 4.0f));
            t = wrap_pi(theta + HALFPI + A);
            u = wrap_pi(A);
            v = wrap_pi(phi - t + 2.0f * u);
        } else {
            A = std::acos(clamp1((rho - 2.0f) / 4.0f));
            t = wrap_pi(theta + HALFPI - A);
            u = wrap_pi(PI - A);
            v = wrap_pi(phi - t + 2.0f * u);
        }

        path.push_back(NonholonomicPathElement(t, Steering::LEFT,  Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::RIGHT, Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::LEFT,  Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v, Steering::RIGHT, Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path7(float x, float y, float phi) {
    // Formula 8.8: C|CuCu|C
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x + std::sin(phi);
    float eta = y - 1.0f - std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    float u1 = (20.0f - rho * rho) / 16.0f;

    if (rho <= 6.0f && u1 >= 0.0f && u1 <= 1.0f) {
        float u = std::acos(clamp1(u1));
        float A = std::asin(clamp1((2.0f * std::sin(u)) / rho));
        float t = wrap_pi(theta + HALFPI + A);
        float v = wrap_pi(t - phi);

        path.push_back(NonholonomicPathElement(t, Steering::LEFT,  Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u, Steering::RIGHT, Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(u, Steering::LEFT,  Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v, Steering::RIGHT, Gear::FORWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path8(float x, float y, float phi) {
    // Formula 8.9 (1): C|C[pi/2]SC
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x - std::sin(phi);
    float eta = y - 1.0f + std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho >= 2.0f) {
        float u = std::sqrt(rho * rho - 4.0f) - 2.0f;
        float A = std::atan2(2.0f, u + 2.0f);
        float t = wrap_pi(theta + HALFPI + A);
        float v = wrap_pi(t - phi + HALFPI);

        path.push_back(NonholonomicPathElement(t,       Steering::LEFT,     Gear::FORWARD));
        path.push_back(NonholonomicPathElement(HALFPI,  Steering::RIGHT,    Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(u,       Steering::STRAIGHT, Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v,       Steering::LEFT,     Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path9(float x, float y, float phi) {
    // Formula 8.9 (2): CSC[pi/2]|C
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x - std::sin(phi);
    float eta = y - 1.0f + std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho >= 2.0f) {
        float u = std::sqrt(rho * rho - 4.0f) - 2.0f;
        float A = std::atan2(u + 2.0f, 2.0f);
        float t = wrap_pi(theta + HALFPI - A);
        float v = wrap_pi(t - phi - HALFPI);

        path.push_back(NonholonomicPathElement(t,       Steering::LEFT,     Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u,       Steering::STRAIGHT, Gear::FORWARD));
        path.push_back(NonholonomicPathElement(HALFPI,  Steering::RIGHT,    Gear::FORWARD));
        path.push_back(NonholonomicPathElement(v,       Steering::LEFT,     Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path10(float x, float y, float phi) {
    // Formula 8.10 (1): C|C[pi/2]SC
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x + std::sin(phi);
    float eta = y - 1.0f - std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho >= 2.0f) {
        float t = wrap_pi(theta + HALFPI);
        float u = rho - 2.0f;
        float v = wrap_pi(phi - t - HALFPI);

        path.push_back(NonholonomicPathElement(t,       Steering::LEFT,     Gear::FORWARD));
        path.push_back(NonholonomicPathElement(HALFPI,  Steering::RIGHT,    Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(u,       Steering::STRAIGHT, Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v,       Steering::RIGHT,    Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path11(float x, float y, float phi) {
    // Formula 8.10 (2): CSC[pi/2]|C
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x + std::sin(phi);
    float eta = y - 1.0f - std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho >= 2.0f) {
        float t = wrap_pi(theta);
        float u = rho - 2.0f;
        float v = wrap_pi(phi - t - HALFPI);

        path.push_back(NonholonomicPathElement(t,       Steering::LEFT,     Gear::FORWARD));
        path.push_back(NonholonomicPathElement(u,       Steering::STRAIGHT, Gear::FORWARD));
        path.push_back(NonholonomicPathElement(HALFPI,  Steering::LEFT,     Gear::FORWARD));
        path.push_back(NonholonomicPathElement(v,       Steering::RIGHT,    Gear::BACKWARD));
    }
    return path;
}

std::vector<NonholonomicPathElement> ReedsShepp::path12(float x, float y, float phi) {
    // Formula 8.11: C|C[pi/2]SC[pi/2]|C
    std::vector<NonholonomicPathElement> path;
    phi = wrap_pi(phi);

    float xi  = x + std::sin(phi);
    float eta = y - 1.0f - std::cos(phi);
    glm::vec2 polar = cart_to_polar(glm::vec2(xi, eta));
    float rho   = polar.x;
    float theta = polar.y;

    if (rho >= 4.0f) {
        float u = std::sqrt(rho * rho - 4.0f) - 4.0f;
        float A = std::atan2(2.0f, u + 4.0f);
        float t = wrap_pi(theta + HALFPI + A);
        float v = wrap_pi(t - phi);

        path.push_back(NonholonomicPathElement(t,       Steering::LEFT,     Gear::FORWARD));
        path.push_back(NonholonomicPathElement(HALFPI,  Steering::RIGHT,    Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(u,       Steering::STRAIGHT, Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(HALFPI,  Steering::LEFT,     Gear::BACKWARD));
        path.push_back(NonholonomicPathElement(v,       Steering::RIGHT,    Gear::FORWARD));
    }
    return path;
}


float ReedsShepp::mod2pi(float a) {
    constexpr float TWO_PI = 2.0f * PI;
    a = std::fmod(a, TWO_PI);
    if (a < 0.0f) a += TWO_PI;
    return a; // [0, 2pi)
}


bool ReedsShepp::dubins_LSL(float alpha, float beta, float d, float& t, float& p, float& q) {
    float sa = std::sin(alpha), sb = std::sin(beta);
    float ca = std::cos(alpha), cb = std::cos(beta);
    float cab = std::cos(alpha - beta);

    float tmp0 = d + sa - sb;
    float p2 = 2.0f + d*d - 2.0f*cab + 2.0f*d*(sa - sb);
    if (p2 < 0.0f) return false;

    float tmp1 = std::atan2((cb - ca), tmp0);
    t = mod2pi(-alpha + tmp1);
    p = std::sqrt(p2);
    q = mod2pi(beta - tmp1);
    return true;
}

bool ReedsShepp::dubins_RSR(float alpha, float beta, float d, float& t, float& p, float& q) {
    float sa = std::sin(alpha), sb = std::sin(beta);
    float ca = std::cos(alpha), cb = std::cos(beta);
    float cab = std::cos(alpha - beta);

    float tmp0 = d - sa + sb;
    float p2 = 2.0f + d*d - 2.0f*cab + 2.0f*d*(sb - sa);
    if (p2 < 0.0f) return false;

    float tmp1 = std::atan2((ca - cb), tmp0);
    t = mod2pi(alpha - tmp1);
    p = std::sqrt(p2);
    q = mod2pi(-beta + tmp1);
    return true;
}

bool ReedsShepp::dubins_LSR(float alpha, float beta, float d, float& t, float& p, float& q) {
    float sa = std::sin(alpha), sb = std::sin(beta);
    float ca = std::cos(alpha), cb = std::cos(beta);
    float cab = std::cos(alpha - beta);

    float p2 = -2.0f + d*d + 2.0f*cab + 2.0f*d*(sa + sb);
    if (p2 < 0.0f) return false;

    p = std::sqrt(p2);
    float tmp2 = std::atan2((-ca - cb), (d + sa + sb)) - std::atan2(-2.0f, p);
    t = mod2pi(-alpha + tmp2);
    q = mod2pi(-mod2pi(beta) + tmp2);
    return true;
}

bool ReedsShepp::dubins_RSL(float alpha, float beta, float d, float& t, float& p, float& q) {
    float sa = std::sin(alpha), sb = std::sin(beta);
    float ca = std::cos(alpha), cb = std::cos(beta);
    float cab = std::cos(alpha - beta);

    float p2 = d*d - 2.0f + 2.0f*cab - 2.0f*d*(sa + sb);
    if (p2 < 0.0f) return false;

    p = std::sqrt(p2);
    float tmp2 = std::atan2((ca + cb), (d - sa - sb)) - std::atan2(2.0f, p);
    t = mod2pi(alpha - tmp2);
    q = mod2pi(beta - tmp2);
    return true;
}

bool ReedsShepp::dubins_RLR(float alpha, float beta, float d, float& t, float& p, float& q) {
    float sa = std::sin(alpha), sb = std::sin(beta);
    float ca = std::cos(alpha), cb = std::cos(beta);
    float cab = std::cos(alpha - beta);

    float tmp = (6.0f - d*d + 2.0f*cab + 2.0f*d*(sa - sb)) / 8.0f;
    if (std::abs(tmp) > 1.0f) return false;

    p = mod2pi(2.0f*PI - std::acos(tmp));
    t = mod2pi(alpha - std::atan2(ca - cb, d - sa + sb) + mod2pi(p * 0.5f));
    q = mod2pi(alpha - beta - t + mod2pi(p));
    return true;
}

bool ReedsShepp::dubins_LRL(float alpha, float beta, float d, float& t, float& p, float& q) {
    float sa = std::sin(alpha), sb = std::sin(beta);
    float ca = std::cos(alpha), cb = std::cos(beta);
    float cab = std::cos(alpha - beta);

    float tmp = (6.0f - d*d + 2.0f*cab + 2.0f*d*(-sa + sb)) / 8.0f;
    if (std::abs(tmp) > 1.0f) return false;

    p = mod2pi(2.0f*PI - std::acos(tmp));
    t = mod2pi(-alpha - std::atan2(ca - cb, d + sa - sb) + p * 0.5f);
    q = mod2pi(mod2pi(beta) - alpha - t + mod2pi(p));
    return true;
}

std::vector<std::vector<NonholonomicPathElement>>
ReedsShepp::get_all_dubins_paths(NonholonomicPos start, NonholonomicPos end, float min_radius) {
    // Keep the same convention you use for Reeds–Shepp so discretize_path() matches.
    start.pos.z = -start.pos.z;
    start.theta = wrap_pi(-start.theta);

    end.pos.z = -end.pos.z;
    end.theta = wrap_pi(-end.theta);

    NonholonomicPos local = change_of_basis_r(start, end, min_radius);

    float x   = local.pos.x;
    float y   = local.pos.z;
    float phi = local.theta; // not wrapped yet

    float D = std::hypot(x, y);
    float d = D; // already normalized by min_radius in change_of_basis_r()

    float theta = mod2pi(std::atan2(y, x));
    float alpha = mod2pi(-theta);
    float beta  = mod2pi(phi - theta);

    std::vector<std::vector<NonholonomicPathElement>> out;
    out.reserve(6);

    auto push3 = [&](float t, float p, float q, Steering a, Steering b, Steering c) {
        std::vector<NonholonomicPathElement> path;
        path.emplace_back(t, a, Gear::FORWARD);
        path.emplace_back(p, b, Gear::FORWARD);
        path.emplace_back(q, c, Gear::FORWARD);
        remove_zero_segments(path);
        if (!path.empty()) out.push_back(std::move(path));
    };

    float t, p, q;

    if (dubins_LSL(alpha, beta, d, t, p, q)) push3(t, p, q, Steering::LEFT,  Steering::STRAIGHT, Steering::LEFT);
    if (dubins_RSR(alpha, beta, d, t, p, q)) push3(t, p, q, Steering::RIGHT, Steering::STRAIGHT, Steering::RIGHT);
    if (dubins_LSR(alpha, beta, d, t, p, q)) push3(t, p, q, Steering::LEFT,  Steering::STRAIGHT, Steering::RIGHT);
    if (dubins_RSL(alpha, beta, d, t, p, q)) push3(t, p, q, Steering::RIGHT, Steering::STRAIGHT, Steering::LEFT);
    if (dubins_RLR(alpha, beta, d, t, p, q)) push3(t, p, q, Steering::RIGHT, Steering::LEFT,     Steering::RIGHT);
    if (dubins_LRL(alpha, beta, d, t, p, q)) push3(t, p, q, Steering::LEFT,  Steering::RIGHT,    Steering::LEFT);

    return out;
}

std::vector<NonholonomicPathElement>
ReedsShepp::get_optimal_dubins_path(NonholonomicPos start, NonholonomicPos end, float min_radius) {
    auto paths = get_all_dubins_paths(start, end, min_radius);
    if (paths.empty()) return {};

    int best = 0;
    float best_len = get_length(paths[0]);

    for (int i = 1; i < (int)paths.size(); i++) {
        float len = get_length(paths[i]);
        if (len < best_len) { best_len = len; best = i; }
    }
    return paths[best];
}

std::vector<NonholonomicPos>
ReedsShepp::get_optimal_dubins_path_discretized(
    NonholonomicPos start, NonholonomicPos end,
    int points_per_circlar_element, float min_radius
) {
    auto path = get_optimal_dubins_path(start, end, min_radius);
    return discretize_path(start, path, points_per_circlar_element, min_radius);
}