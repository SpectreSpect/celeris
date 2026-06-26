#include "nonholonomic_a_star.h"


namespace rs_detail {

static constexpr double PI = 3.1415926535897932384626433832795;
static constexpr double EPS = 1e-10;

static inline double mod2pi(double a) {
    a = std::fmod(a, 2.0 * PI);
    if (a < 0) a += 2.0 * PI;
    return a; // [0, 2pi)
}

static inline double wrap_pi(double a) {
    a = mod2pi(a);
    if (a > PI) a -= 2.0 * PI;
    return a; // (-pi, pi]
}

static inline void polar(double x, double y, double &r, double &theta) {
    r = std::hypot(x, y);
    theta = std::atan2(y, x);
}

enum Seg : int { L = 0, R = 1, S = 2, N = 3 };

struct Candidate {
    std::array<Seg,5> type {N,N,N,N,N};
    std::array<double,5> len {0,0,0,0,0}; // signed (sign => forward/back)
    double total = std::numeric_limits<double>::infinity();
};

static inline double total_len(const Candidate& c) {
    double s = 0.0;
    for (int i=0;i<5;i++) {
        if (c.type[i] == N) break;
        s += std::abs(c.len[i]);
    }
    return s;
}

static inline void try_update(Candidate& best, const std::array<Seg,5>& type,
                              const std::array<double,5>& len)
{
    Candidate c;
    c.type = type;
    c.len  = len;
    c.total = total_len(c);
    if (c.total + 1e-12 < best.total) best = c;
}

// --- Primitive solvers (same math as standard RS derivations) ---

// L+ S+ L+  (works with signed lengths via transforms outside)
static inline bool LpSpLp(double x, double y, double phi, double& t, double& u, double& v) {
    double xi  = x - std::sin(phi);
    double eta = y - 1.0 + std::cos(phi);
    polar(xi, eta, u, t);
    v = mod2pi(phi - t);
    return (t >= -EPS) && (u >= -EPS) && (v >= -EPS);
}

// L+ S+ R+
static inline bool LpSpRp(double x, double y, double phi, double& t, double& u, double& v) {
    double xi  = x + std::sin(phi);
    double eta = y - 1.0 - std::cos(phi);
    double r, theta;
    polar(xi, eta, r, theta);
    if (r < 2.0 - EPS) return false;
    u = std::sqrt(std::max(0.0, r*r - 4.0));
    double ang = std::atan2(2.0, u);
    t = mod2pi(theta + ang);
    v = mod2pi(t - phi);
    return (t >= -EPS) && (u >= -EPS) && (v >= -EPS);
}

// L+ R- L+   (CCC family core)
static inline bool LpRmL(double x, double y, double phi, double& t, double& u, double& v) {
    // One common analytic form (covers the usual CCC cases).
    // Note: u will be negative here (R-).
    double xi  = x - std::sin(phi);
    double eta = y - 1.0 + std::cos(phi);
    double r, theta;
    polar(xi, eta, r, theta);
    if (r > 4.0 + EPS) return false;

    u = -2.0 * std::asin(std::clamp(r / 4.0, -1.0, 1.0)); // negative
    t = mod2pi(theta + 0.5*u + PI);
    v = mod2pi(phi - t + u);
    return (t >= -EPS) && (v >= -EPS);
}

// --- Families: build candidates using timeflip/reflect symmetries ---

static inline void CSC(double x, double y, double phi, Candidate& best) {
    double t,u,v;

    // LSL
    if (LpSpLp(x, y, phi, t,u,v)) {
        try_update(best, {L,S,L,N,N}, { t, u, v, 0,0 });
    }
    // timeflip (reverse time) -> negate lengths
    if (LpSpLp(-x, y, -phi, t,u,v)) {
        try_update(best, {L,S,L,N,N}, { -t, -u, -v, 0,0 });
    }
    // reflect (mirror) swaps L<->R
    if (LpSpLp(x, -y, -phi, t,u,v)) {
        try_update(best, {R,S,R,N,N}, { t, u, v, 0,0 });
    }
    // timeflip + reflect
    if (LpSpLp(-x, -y, phi, t,u,v)) {
        try_update(best, {R,S,R,N,N}, { -t, -u, -v, 0,0 });
    }

    // LSR
    if (LpSpRp(x, y, phi, t,u,v)) {
        try_update(best, {L,S,R,N,N}, { t, u, v, 0,0 });
    }
    if (LpSpRp(-x, y, -phi, t,u,v)) {
        try_update(best, {L,S,R,N,N}, { -t, -u, -v, 0,0 });
    }
    // reflect => RSL
    if (LpSpRp(x, -y, -phi, t,u,v)) {
        try_update(best, {R,S,L,N,N}, { t, u, v, 0,0 });
    }
    if (LpSpRp(-x, -y, phi, t,u,v)) {
        try_update(best, {R,S,L,N,N}, { -t, -u, -v, 0,0 });
    }
}

static inline void CCC(double x, double y, double phi, Candidate& best) {
    double t,u,v;

    // L R L  (with middle being R- via u<0)
    if (LpRmL(x, y, phi, t,u,v)) {
        try_update(best, {L,R,L,N,N}, { t, u, v, 0,0 });
    }
    if (LpRmL(-x, y, -phi, t,u,v)) {
        try_update(best, {L,R,L,N,N}, { -t, -u, -v, 0,0 });
    }
    // reflect => R L R
    if (LpRmL(x, -y, -phi, t,u,v)) {
        try_update(best, {R,L,R,N,N}, { t, u, v, 0,0 });
    }
    if (LpRmL(-x, -y, phi, t,u,v)) {
        try_update(best, {R,L,R,N,N}, { -t, -u, -v, 0,0 });
    }
}

static inline Candidate solve(double x, double y, double phi) {
    Candidate best;
    CSC(x,y,phi,best);
    CCC(x,y,phi,best);
    return best;
}

// Integrate one small signed step ds (unit radius) for segment type.
static inline void step_local(Seg seg, double ds, double& x, double& y, double& yaw) {
    if (std::abs(ds) < 1e-15) return;

    if (seg == S) {
        x += ds * std::cos(yaw);
        y += ds * std::sin(yaw);
        return;
    }

    int k = (seg == L) ? +1 : -1; // curvature sign
    double yaw0 = yaw;
    double yaw1 = yaw0 + k * ds;

    // x += (sin(yaw1)-sin(yaw0))/k ; y += (-cos(yaw1)+cos(yaw0))/k
    x += (std::sin(yaw1) - std::sin(yaw0)) / double(k);
    y += (-std::cos(yaw1) + std::cos(yaw0)) / double(k);
    yaw = mod2pi(yaw1);
}

} // namespace rs_detail


// ------------------------------------------------------------
// Your requested function signature.
// Set min_turn_radius + step to your liking inside.
// ------------------------------------------------------------
std::vector<NonholonomicPos> NonholonomicAStar::find_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos)
{
    // TODO: set this to your car’s minimum turning radius (world units).
    // If you have wheel_base and max_steer (radians): rho = wheel_base / tan(max_steer)
    const float tanMax = std::tan(max_steer);
    if (std::abs(tanMax) < 1e-6f) return {};    // can't turn -> no RS

    const float min_turn_radius = wheel_base / tanMax;

    // Sampling resolution along the path (world units)
    const float step_world = reeds_shepp_step_world;

    // Work in XZ plane (like your simulator). y is kept from start_pos.
    glm::vec3 d = end_pos.pos - start_pos.pos;

    double th0 = (double)start_pos.theta;
    double c0  = std::cos(th0);
    double s0  = std::sin(th0);

    // Rotate world delta into start frame and normalize by rho
    double dx = (double)d.x;
    double dy = (double)d.z; // Z acts like "y" in 2D formulas

    double x = ( c0*dx + s0*dy ) / (double)min_turn_radius;
    double y = (-s0*dx + c0*dy ) / (double)min_turn_radius;
    // double phi = rs_detail::mod2pi((double)end_pos.theta - th0);
    double phi = rs_detail::wrap_pi((double)end_pos.theta - th0);

    // Solve (local, unit radius)
    rs_detail::Candidate best = rs_detail::solve(x, y, phi);
    if (!std::isfinite(best.total)) return {};

    // Sample the best path in local frame, then transform back to world.
    std::vector<NonholonomicPos> out;
    out.reserve((size_t)(best.total * (double)min_turn_radius / (double)step_world) + 8);

    double lx = 0.0, ly = 0.0, yaw = 0.0;

    auto emit_world = [&](rs_detail::Seg seg, double ds_signed)
    {
        NonholonomicPos p;
        // inverse rotation + scale back:
        // world_dx = cos(th0)*lx - sin(th0)*ly
        // world_dz = sin(th0)*lx + cos(th0)*ly
        double wdx = (c0*lx - s0*ly) * (double)min_turn_radius;
        double wdz = (s0*lx + c0*ly) * (double)min_turn_radius;

        p.pos   = start_pos.pos + glm::vec3((float)wdx, 0.0f, (float)wdz);
        p.pos.y = start_pos.pos.y; // keep altitude; change if you want

        // p.theta = (float)rs_detail::mod2pi(th0 + yaw);
        p.theta = (float)rs_detail::wrap_pi(th0 + yaw);

        // steer convention: -1 left, +1 right, 0 straight
        if (seg == rs_detail::L) p.steer = -1.0f;
        else if (seg == rs_detail::R) p.steer = +1.0f;
        else p.steer = 0.0f;

        p.dir = (ds_signed >= 0.0) ? 1.0f : -1.0f;

        out.push_back(p);
    };

    // include the start pose
    {
        NonholonomicPos p = start_pos;
        p.steer = 0.0f;
        p.dir   = 1.0f;
        out.push_back(p);
    }

    const double step_u = (double)step_world / (double)min_turn_radius;

    for (int i = 0; i < 5; ++i) {
        if (best.type[i] == rs_detail::N) break;
        rs_detail::Seg seg = best.type[i];
        double L = best.len[i];
        if (std::abs(L) < 1e-12) continue;

        double sgn = (L >= 0.0) ? 1.0 : -1.0;
        double rem = std::abs(L);

        while (rem > 1e-12) {
            double ds = std::min(step_u, rem);
            rem -= ds;
            ds *= sgn; // signed step

            rs_detail::step_local(seg, ds, lx, ly, yaw);
            emit_world(seg, ds);
        }
    }

    // Force the last pose to match the requested end (nice for debugging)
    if (!out.empty()) {
        out.back().pos   = end_pos.pos;
        out.back().theta = end_pos.theta;
    }
    return out;
}


double NonholonomicAStar::reeds_shepp_distance(
    NonholonomicPos& start_pos,
    NonholonomicPos& end_pos
) {
    const double tanMax = std::tan((double)max_steer);
    if (std::abs(tanMax) < 1e-12) {
        return std::numeric_limits<double>::infinity();
    }
    const double rho = (double)wheel_base / tanMax;

    glm::vec3 d = end_pos.pos - start_pos.pos;

    const double th0 = (double)start_pos.theta;
    const double c0  = std::cos(th0);
    const double s0  = std::sin(th0);

    // XZ plane -> local start frame -> normalize by rho
    const double dx = (double)d.x;
    const double dy = (double)d.z;

    const double x   = ( c0*dx + s0*dy ) / rho;
    const double y   = (-s0*dx + c0*dy ) / rho;
    // const double phi = rs_detail::mod2pi((double)end_pos.theta - th0);
    const double phi = rs_detail::wrap_pi((double)end_pos.theta - th0);

    rs_detail::Candidate best = rs_detail::solve(x, y, phi);
    if (!std::isfinite(best.total)) {
        return std::numeric_limits<double>::infinity();
    }
    return best.total * rho;
}

NonholonomicAStar::NonholonomicAStar(VoxelGrid* voxel_grid) {
    this->grid = new VoxelOccupancyGrid3D(voxel_grid);
}

std::vector<NonholonomicPos> NonholonomicAStar::simulate_motion(NonholonomicPos start, int steer, int direction)
{
    steer = std::clamp(steer, -1, 1);
    direction = (direction < 0) ? -1 : 1; // only -1 or +1

    float delta = 0.0f;
    if (steer != 0) delta = -steer * max_steer;

    std::vector<NonholonomicPos> out;
    out.reserve(integration_steps);

    glm::vec3 p = start.pos;
    float yaw   = start.theta;

    const float ds = motion_simulation_dist / float(integration_steps);
    const float ds_signed = ds * float(direction);
    const float eps = 1e-6f;

    for (int i = 0; i < integration_steps; ++i) {
        if (std::abs(delta) < eps) {
            // straight (forward or reverse)
            p.x += ds_signed * std::cos(yaw);
            p.z += ds_signed * std::sin(yaw);
        } else {
            // exact circular arc integration (forward or reverse)
            float tanD = std::tan(delta);
            // (tanD won't be ~0 here because of the eps check above)
            float R = wheel_base / tanD;   // signed radius
            float yaw0 = yaw;
            float dYaw = ds_signed / R;    // signed
            yaw += dYaw;

            p.x += R * (std::sin(yaw) - std::sin(yaw0));
            p.z += R * (-std::cos(yaw) + std::cos(yaw0));
        }

        NonholonomicPos s = start;
        s.pos = p;
        s.theta = yaw;
        out.push_back(s);
    }

    return out;
}

bool NonholonomicAStar::adjust_and_check_path(std::vector<NonholonomicPos>& path, int max_step_up, int max_drop) {
    if (path.size() > 0)
    for (int i = 0; i < path.size(); i++) {
        glm::vec3 voxel_pos = glm::vec3(glm::floor(path[i].pos));
        if (!grid->adjust_to_ground(voxel_pos, max_step_up, max_drop, max_y_diff)) {
            return false;
        }
            
        // IMPORTANT: depending on your convention you may want +0.5f or +1.0f here
        path[i].pos.y = (float)voxel_pos.y;
        if (i + 1 < path.size())
            path[i + 1].pos.y = path[i].pos.y;
    }
    return true;
}

bool NonholonomicAStar::almost_equal(NonholonomicPos a, NonholonomicPos b) {
    float dist = glm::distance(a.pos, b.pos);
    float theta_diff = std::abs(angle_diff(a.theta, b.theta));

    // std::cout << theta_diff << std::endl;

    bool almost_equal_pos = dist <= 0.7f;
    bool almost_equal_angle = theta_diff <= 0.3f;

    // std::cout << dist << " " << theta_diff << std::endl;

    return almost_equal_pos && almost_equal_angle;
}

std::vector<NonholonomicPos> NonholonomicAStar::reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos) {
    std::vector<NonholonomicPos> path;
    NonholonomicPos cur_pos = pos;

    while (true) {
        uint64_t cur_key = state_key(cur_pos);

        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        NonholonomicAStarCell prev_cell = it->second;

        path.push_back(cur_pos);
        
        if (prev_cell.no_parent)
            break;
        
        cur_pos = prev_cell.came_from;
    }

    std::reverse(path.begin(), path.end());

    return path;
}

DistToPathData NonholonomicAStar::dist_to_path(glm::ivec3 pos, std::vector<glm::ivec3>& path) {
    int id = 0;
    float min_dist = 0;
    if (path.size() > 0) {
        glm::vec3 cur_pos = (glm::vec3)pos;
        glm::vec3 path_pos = (glm::vec3)path[id];
        path_pos.y = cur_pos.y;

        min_dist = glm::distance((glm::vec3)pos, (glm::vec3)path[id]);

    }

    for (int i = 0; i < path.size(); i++) {
        glm::vec3 cur_pos = (glm::vec3)pos;
        glm::vec3 path_pos = (glm::vec3)path[i];

        path_pos.y = cur_pos.y;

        std::vector<glm::vec3> line = {cur_pos, path_pos};


        std::vector<glm::ivec3> ground_positions;
        if (!grid->get_ground_positions(line, ground_positions, max_step_up, max_drop, max_y_diff))
            continue;
        
        if (!crosses_extreme_curvature(ground_positions, curvature_limit))
            continue;    

        float cur_dist = glm::distance(cur_pos, path_pos);
        
        if (cur_dist < min_dist) {
            min_dist = cur_dist;
            id = i;
        }     
    }


    DistToPathData dist_to_path_data;
    dist_to_path_data.dist = min_dist;
    dist_to_path_data.id = id;

    return dist_to_path_data;
}

DistToPathData NonholonomicAStar::max_unimpended_dist_to_path(glm::vec3 pos, std::vector<glm::ivec3>& path, int start_id, glm::vec3 last_pos, bool replace_last_pos) {
    int id = -1;
    float max_dist = 0;

    glm::vec3 cur_pos = (glm::vec3)pos;
    for (int i = start_id; i < path.size(); i++) {
        glm::vec3 path_pos = (glm::vec3)path[i] + glm::vec3(0.5f, 0.0f, 0.5f);
        // glm::vec3 path_pos = (glm::vec3)path[i];

        if (replace_last_pos && i == path.size() - 1)
            path_pos = last_pos;
            

        path_pos.y = cur_pos.y;

        std::vector<glm::vec3> line = {cur_pos, path_pos};

        std::vector<glm::ivec3> ground_positions;
        if (!grid->get_ground_positions(line, ground_positions, max_step_up, max_drop, max_y_diff))
            continue;
        
        if (!crosses_extreme_curvature(ground_positions, curvature_limit))
            continue;    
        id = i;
        max_dist = glm::distance(cur_pos, path_pos); 
    }


    DistToPathData dist_to_path_data;
    dist_to_path_data.dist = max_dist;
    dist_to_path_data.id = id;

    return dist_to_path_data;
}

DistToPathData NonholonomicAStar::dist_to_path_dubins(NonholonomicPos pos, std::vector<glm::ivec3>& path) {
    int id = 0;
    float min_dist = 0;
    if (path.size() > 0) {
        glm::vec3 cur_pos = (glm::vec3)pos.pos;

        glm::vec3 dir = (glm::vec3)path[id+1] - (glm::vec3)path[id+1];
        NonholonomicPos path_pos;
        path_pos.pos = (glm::vec3)path[id];
        path_pos.theta = std::atan2(dir.z, dir.x);

        // const float min_radius = wheel_base / std::tan(max_steer);
        std::vector<NonholonomicPathElement> dubins_path = ReedsShepp::get_optimal_dubins_path(pos, path_pos, min_radius);
        min_dist = ReedsShepp::get_length(dubins_path);
    }

    for (int i = 0; i < path.size(); i++) {
        glm::vec3 cur_pos = (glm::vec3)pos.pos;
        glm::vec3 path_pos = (glm::vec3)path[i];

        float cur_dist = glm::distance(cur_pos, path_pos);
        
        if (cur_dist < min_dist) {
            min_dist = cur_dist;
            id = i;
        }     
    }

    DistToPathData dist_to_path_data;
    dist_to_path_data.dist = min_dist;
    dist_to_path_data.id = id;

    return dist_to_path_data;
}

float NonholonomicAStar::follow_plain_astar_heuristic(glm::ivec3 pos, std::vector<glm::ivec3>& path, float scale, float dist_to_path_threshold) {
    DistToPathData dist_to_path_data = dist_to_path(pos, path);

    float id_factor = path.size();

    if (dist_to_path_data.dist < dist_to_path_threshold) {
        id_factor -= (dist_to_path_data.id + 1);
    }

    id_factor = ((float)id_factor / path.size()) * scale;

    return id_factor;
}

int NonholonomicAStar::discretize_angle(float value, int num_bins) {
    if (num_bins <= 0) return 0;

    const float TWO_PI = 6.2831853071795864769f;

    float a = std::fmod(value, TWO_PI);
    if (a < 0.0f) a += TWO_PI;

    float t = a / TWO_PI;                 // [0,1)
    int bin = (int)std::floor(t * num_bins + 0.5f); // round-to-nearest
    bin %= num_bins;                      // wrap in case it hits num_bins
    return bin;
}

float NonholonomicAStar::get_nonholonomic_f(NonholonomicPos& new_pos, NonholonomicPos end_pos, NonholonomicPos cur_pos, PlainAstarData plain_a_star_path) {
    // new_pos.pos.y = 0;
    // end_pos.pos.y = 0;

    // float follow_plain_astar_f = 0;
    // float plain_a_star_progress = 0;
    // float close_to_end = false;
    // DistToPathData dist_to_path_data;
    // dist_to_path_data.id = -1;
    // DistToPathData unimpended_dist;
    // unimpended_dist.id = -1;
    // if (plain_a_star_path.path.size() > 0) {
    //     dist_to_path_data = dist_to_path(new_pos.pos, plain_a_star_path.path);
    //     if (dist_to_path_data.id == -1)
    //         follow_plain_astar_f = 0;
    //     else
    //         follow_plain_astar_f = plain_a_star_path.dist_to_end[dist_to_path_data.id] + dist_to_path_data.dist;
        
    //     if (dist_to_path_data.id / plain_a_star_path.dist_to_end.size() >= 0.8f)
    //         close_to_end = true;
    // }

    // const float min_radius = wheel_base / std::tan(max_steer);

    // int target_id = unimpended_dist.id;

    
    new_pos.dubins_segment_id = cur_pos.dubins_segment_id;
    if (cur_pos.dubins_segment_id < unimpended_astar_positions.size() - 1)
        if (glm::distance(new_pos.pos, unimpended_astar_positions[cur_pos.dubins_segment_id].pos) <= 1.0f) {
            new_pos.dubins_segment_id += 1;
    }

    float f = dubins_distance_to_end[new_pos.dubins_segment_id];

    if (new_pos.dubins_segment_id >= unimpended_astar_positions.size() - 1) {
        std::vector<NonholonomicPathElement> dubins_path;
        dubins_path = ReedsShepp::get_optimal_path(new_pos, unimpended_astar_positions[new_pos.dubins_segment_id], min_radius);
        f += ReedsShepp::get_length(dubins_path) * min_radius;
        force_reeds_shepp_shot = true;

    } else if (dubins_distance_to_end[new_pos.dubins_segment_id - 1] <= 2.0 || glm::distance(new_pos.pos, unimpended_astar_positions[new_pos.dubins_segment_id].pos) <= 2.0f) {
        f += glm::distance(new_pos.pos, unimpended_astar_positions[new_pos.dubins_segment_id].pos);
    }
    else{
        std::vector<NonholonomicPathElement> dubins_path;
        dubins_path = ReedsShepp::get_optimal_dubins_path(new_pos, unimpended_astar_positions[new_pos.dubins_segment_id], min_radius);
        f += ReedsShepp::get_length(dubins_path) * min_radius;
    }

    return f;
}

void NonholonomicAStar::initialize(NonholonomicPos start_pos, NonholonomicPos end_pos) {
    state_pq = std::priority_queue<NonholonomicAStarCell, std::vector<NonholonomicAStarCell>, NonholonomicByPriority>();
    state_closed_heap = std::unordered_map<uint64_t, NonholonomicAStarCell>();
    state_g_score = std::unordered_map<uint64_t, float>();
    state_path = std::vector<NonholonomicPos>();
    state_explored_paths = std::vector<LineInstance>();
    state_start_pos = start_pos;
    state_end_pos = end_pos;
    state_counter = 0;
    
    min_radius = wheel_base / std::tan(max_steer);

    state_plain_astar_path = find_path(glm::ivec3(glm::floor(start_pos.pos)), glm::ivec3(glm::floor(end_pos.pos)));

    if (state_plain_astar_path.path.empty())
        return;

    unimpended_astar_positions = std::vector<NonholonomicPos>();

    NonholonomicPos first_unimpended_pos = start_pos;
    first_unimpended_pos.pos.y = state_plain_astar_path.path[0].y;
    unimpended_astar_positions.push_back(first_unimpended_pos);

    // dubins_segment_lengths = std::vector<float>();

    NonholonomicPos cur_pos = start_pos;
    int last_id = -1;
    // int last_id = 0;
    // int stride = 5;
    while(true) {
        DistToPathData unimpended_dist_data = max_unimpended_dist_to_path(cur_pos.pos, state_plain_astar_path.path, last_id + 1, end_pos.pos, true);
        // last_id = std::min((float)last_id + stride, (float)state_plain_astar_path.path.size() - 1);
        // DistToPathData unimpended_dist_data;
        // unimpended_dist_data.id = last_id;
        

        if (last_id == unimpended_dist_data.id || unimpended_dist_data.id == -1)
            break;

        last_id = unimpended_dist_data.id;

        NonholonomicPos unimpended_pos;
        
        if (unimpended_dist_data.id == state_plain_astar_path.path.size() - 1) {
            NonholonomicPos prev_pos = unimpended_astar_positions[unimpended_astar_positions.size() - 1];

            float ratio = 0.7;
            glm::vec3 mid_pos_vec = prev_pos.pos * (1 - ratio) + end_pos.pos * ratio;
            grid->adjust_to_ground(mid_pos_vec, max_step_up, max_drop, max_y_diff);

            
            NonholonomicPos mid_pos;
            mid_pos.pos = mid_pos_vec;
            mid_pos.theta = prev_pos.theta;

            // grid->adjust_to_ground(mid_pos, max_step_up, max_drop, max_y_diff);

            unimpended_astar_positions.push_back(mid_pos);
            // float dist = glm::distance(prev_pos.pos, mid_pos.pos);
            // if (dist > 2.0f) {
            //     std::vector<NonholonomicPathElement> dubins_path = ReedsShepp::get_optimal_dubins_path(prev_pos, mid_pos, min_radius);
            //     dist = ReedsShepp::get_length(dubins_path) * min_radius;
            // }
            // dubins_segment_lengths.push_back(dist);

            unimpended_pos = end_pos;
            unimpended_astar_positions.push_back(unimpended_pos);

            // dist = glm::distance(mid_pos.pos, unimpended_pos.pos);
            // if (dist > 2.0f) {
            //     std::vector<NonholonomicPathElement> dubins_path = ReedsShepp::get_optimal_dubins_path(mid_pos, unimpended_pos, min_radius);
            //     dist = ReedsShepp::get_length(dubins_path) * min_radius;
            // }

            // dubins_segment_lengths.push_back(dist);

            break;
        }
        glm::vec3 dir = glm::normalize(glm::vec3(state_plain_astar_path.path[unimpended_dist_data.id + 1] - state_plain_astar_path.path[unimpended_dist_data.id]));

        // glm::vec3 dir_1 = glm::normalize(glm::vec3(state_plain_astar_path.path[unimpended_dist_data.id + 1] - state_plain_astar_path.path[unimpended_dist_data.id]));
        // glm::vec3 dir_2 = glm::normalize(glm::vec3(state_plain_astar_path.path[unimpended_dist_data.id + 2] - state_plain_astar_path.path[unimpended_dist_data.id + 1]));
        // glm::vec3 dir = glm::normalize((dir_1 + dir_2) / 2.0f);

        unimpended_pos.pos = state_plain_astar_path.path[unimpended_dist_data.id];   
        unimpended_pos.pos += glm::vec3(0.5f, 0, 0.5f);
        unimpended_pos.theta = std::atan2(dir.z, dir.x);

        unimpended_astar_positions.push_back(unimpended_pos);

        // float dist = glm::distance(cur_pos.pos, unimpended_pos.pos);
        // if (dist > 2.0f) {
        //     std::vector<NonholonomicPathElement> dubins_path = ReedsShepp::get_optimal_dubins_path(cur_pos, unimpended_pos, min_radius);
        //     dist = ReedsShepp::get_length(dubins_path) * min_radius;
        // }

        // dubins_segment_lengths.push_back(dist);

        cur_pos = unimpended_pos;
    }

    // for (int i = 1; i < unimpended_astar_positions.size() - 1; i++) {

    //     if (i < unimpended_astar_positions.size() - 2) {
    //         glm::vec3 vec_1 = glm::normalize(unimpended_astar_positions[i].pos - unimpended_astar_positions[i-1].pos);
    //         glm::vec3 vec_2 = glm::normalize(unimpended_astar_positions[i + 1].pos - unimpended_astar_positions[i].pos);

    //         glm::vec3 displacement_dir = glm::normalize(vec_1 - (vec_1 + vec_2) / 2.0f);

    //         displacement_dir.y = 0;
    //         // displacement_dir.z = 0;
    //         // displacement_dir.x = 0;

    //         unimpended_astar_positions[i].pos += displacement_dir * 5.0f;
    //     }


    //     // glm::vec3 dir = glm::normalize(unimpended_astar_positions[i].pos - unimpended_astar_positions[i-1].pos);
    //     // // glm::vec3 dir = glm::normalize((dir_1 + dir_2) / 2.0f);
    //     // unimpended_astar_positions[i].theta = std::atan2(dir.z, dir.x);
    // }

    // float offset = 5.0f;
    // std::vector<NonholonomicPos> unimpended_astar_positions_new;
    // unimpended_astar_positions_new.reserve(unimpended_astar_positions.size() * 2 - 1);

    // glm::vec3 up = {0, 1, 0};
    // for (size_t i = 0; i < unimpended_astar_positions.size()-1; i++) {
    //     NonholonomicPos& cur_point = unimpended_astar_positions[i];
    //     NonholonomicPos& next_point = unimpended_astar_positions[i + 1];
    //     glm::vec3 center_pos = (next_point.pos + cur_point.pos) / 2.0f;
    //     float base_lenght = glm::distance(next_point.pos, cur_point.pos);
    //     offset = base_lenght / 4.0f;

    //     glm::vec3 a, b;
    //     if (i == 0) {
    //         NonholonomicPos& double_next_point = unimpended_astar_positions[i + 2];
    //         a = cur_point.pos - next_point.pos;
    //         b = double_next_point.pos - next_point.pos;
    //     } else {
    //         NonholonomicPos& prev_point = unimpended_astar_positions[i - 1];
    //         a = prev_point.pos - cur_point.pos;
    //         b = next_point.pos - cur_point.pos;
    //     }

    //     float s = glm::length(glm::cross(a, b));

    //     std::cout << s << std::endl;

    //     glm::vec3 dir = s > 0 ? glm::cross(next_point.pos - cur_point.pos, up) : glm::cross(up, next_point.pos - cur_point.pos) ;
    //     dir = glm::normalize(dir);

    //     NonholonomicPos new_cur_point;
    //     NonholonomicPos new_point; // <-- target

    //     new_cur_point.pos = cur_point.pos;
    //     new_point.pos = center_pos + dir * offset; //!!!

    //     // theta
    //     glm::vec3 cur_dir(0);
    //     if (i != 0)
    //         cur_dir = glm::normalize(cur_point.pos - unimpended_astar_positions_new.back().pos);

    //     glm::vec3 new_dir = glm::normalize(new_point.pos - cur_point.pos);

    //     new_cur_point.theta = i == 0 ? cur_point.theta : std::atan2(cur_dir.z, cur_dir.x);
    //     new_point.theta = std::atan2(new_dir.z, new_dir.x);

    //     unimpended_astar_positions_new.push_back(new_cur_point);
    //     unimpended_astar_positions_new.push_back(new_point);
    // }
    // unimpended_astar_positions_new.push_back(unimpended_astar_positions.back());
    
    // unimpended_astar_positions = std::move(unimpended_astar_positions_new);
    dubins_segment_lengths = std::vector<float>();
    dubins_segment_lengths.reserve(unimpended_astar_positions.size() - 1);

    for (int i = 0; i < unimpended_astar_positions.size() - 1; i++) {
        NonholonomicPos cur_pos = unimpended_astar_positions[i];
        NonholonomicPos next_pos = unimpended_astar_positions[i+1];

        float dist = glm::distance(cur_pos.pos, next_pos.pos);
        if (dist > 2.0f) {
            std::vector<NonholonomicPathElement> dubins_path = ReedsShepp::get_optimal_dubins_path(cur_pos, next_pos, min_radius);
            dist = ReedsShepp::get_length(dubins_path) * min_radius;
        }

        dubins_segment_lengths.push_back(dist);



        // if (i < unimpended_astar_positions.size() - 2) {
        //     glm::vec3 vec_1 = glm::normalize(unimpended_astar_positions[i].pos - unimpended_astar_positions[i-1].pos);
        //     glm::vec3 vec_2 = glm::normalize(unimpended_astar_positions[i + 1].pos - unimpended_astar_positions[i].pos);

        //     glm::vec3 displacement_dir = glm::normalize(vec_1 - (vec_1 + vec_2) / 2.0f);

        //     displacement_dir.y = 0;
        //     // displacement_dir.z = 0;
        //     // displacement_dir.x = 0;

        //     unimpended_astar_positions[i].pos += displacement_dir * 10.0f;
        // }


        // glm::vec3 dir = glm::normalize(unimpended_astar_positions[i].pos - unimpended_astar_positions[i-1].pos);
        // // glm::vec3 dir = glm::normalize((dir_1 + dir_2) / 2.0f);
        // unimpended_astar_positions[i].theta = std::atan2(dir.z, dir.x);
    }

    


    for (int i = 1; i < unimpended_astar_positions.size() - 1; i++) {
        
        glm::vec3 dir = glm::normalize(unimpended_astar_positions[i].pos - unimpended_astar_positions[i-1].pos);
        // glm::vec3 dir = glm::normalize((dir_1 + dir_2) / 2.0f);
        unimpended_astar_positions[i].theta = std::atan2(dir.z, dir.x);
    }

    // float cur_length = 0;
    // for (int i = 0; i < dubins_segment_lengths.size(); i++) {
    //     int id = (dubins_segment_lengths.size() - 1) - i;
    //     cur_length += dubins_segment_lengths[i];

    //     dubins_distance_to_end.push_back(cur_length);
    // }
    // std::reverse(dubins_distance_to_end.begin(), dubins_distance_to_end.end());

    dubins_distance_to_end = std::vector<float>(dubins_segment_lengths.size(), 0.0f);
    float cur = 0.0f;
    for (int i = (int)dubins_segment_lengths.size() - 1; i >= 0; --i) {
        cur += dubins_segment_lengths[i];
        dubins_distance_to_end[i] = cur; 
    }

    // std::reverse(dubins_distance_to_end.begin(), dubins_distance_to_end.end());

    state_start_cell = NonholonomicAStarCell();
    state_start_cell.pos = start_pos;
    state_start_cell.no_parent = true;
    state_start_cell.g = 0;
    state_start_cell.f = 99999999;

    state_pq.push(state_start_cell);   

    motion_simulation_time = AvgTimer();
    adjust_to_ground_time = AvgTimer();
    get_ground_positions_time = AvgTimer();
    crosses_extreme_curvature_time = AvgTimer();
    get_nonholonomic_f_time = AvgTimer();
}

bool NonholonomicAStar::crosses_extreme_curvature(const std::vector<NonholonomicPos>& path, float curvature_limit) {
    for (const auto& s : path) {
        glm::ivec3 ground_cell = glm::ivec3(glm::floor(s.pos)) - glm::ivec3(0, 1, 0);
        OccupancyCell cell = grid->get_cell(ground_cell);

        if (cell.curvature >= curvature_limit)
            return false;
    }
    return true;
}

bool NonholonomicAStar::crosses_extreme_curvature(const std::vector<glm::ivec3>& path, float curvature_limit) {
    for (const auto& s : path) {
        glm::ivec3 ground_cell = glm::ivec3(s);
        OccupancyCell cell = grid->get_cell(ground_cell);

        if (cell.curvature >= curvature_limit)
            return false;
    }
    return true;
}

bool NonholonomicAStar::try_reeds_shepp_shot(NonholonomicPos& start, NonholonomicPos& end, std::vector<NonholonomicPos>& out_path) {
    // out_path = find_reeds_shepp(start, end);
    // const float min_radius = wheel_base / std::tan(max_steer);
    out_path = ReedsShepp::get_optimal_path_discretized(start, end, 8, min_radius);

    if (out_path.empty())
        return false;

    if (!grid->adjust_to_ground(out_path, max_step_up, max_drop, max_y_diff))
        return false;

    if (!crosses_extreme_curvature(out_path, curvature_limit))
        return false;
    
    return true;
}

bool NonholonomicAStar::try_finish_with_reeds_shepp(NonholonomicPos& from, NonholonomicPos& to) {
    std::vector<NonholonomicPos> reeds_shepp_path;
    if (try_reeds_shepp_shot(from, to, reeds_shepp_path)) {
        state_path = reconstruct_path(state_closed_heap, from);
        state_path.insert(state_path.end(), reeds_shepp_path.begin(), reeds_shepp_path.end());
        return true;
    }
    return false;
}

bool NonholonomicAStar::find_nonholomic_path_step() {
    if (state_pq.empty()) {
        std::cout << "1. Priotirty queue is empty" << std::endl;
        return true;
    }

    NonholonomicAStarCell cur_cell = state_pq.top();
    state_pq.pop();

    uint64_t cur_key = state_key(cur_cell.pos);
    auto cur_it = state_g_score.find(cur_key);

    if (cur_it != state_g_score.end()) {
        if (cur_cell.g > cur_it->second)
            return false;
    }

    if (track_explored_paths)
        if (!cur_cell.no_parent) {
            LineInstance line_instance;
            line_instance.p0 = cur_cell.pos.pos + glm::vec3(0.0f, 0.2f, 0.0f);
            line_instance.p1 = cur_cell.came_from.pos + glm::vec3(0.0f, 0.2f, 0.0f);

            state_explored_paths.push_back(line_instance);
        }

    if (state_counter >= iteration_limit) {
        std::cout << "Limit exceeded" << std::endl;
        return true;
    }
    
    state_closed_heap[cur_key] = cur_cell;

    if (almost_equal(cur_cell.pos, state_end_pos)) {
        state_path = reconstruct_path(state_closed_heap, cur_cell.pos);

        if (!use_reed_shepps_fallback) {
            std::cout << "2. Almost equal = true" << std::endl;
            return true;
        }

        bool status = try_finish_with_reeds_shepp(cur_cell.pos, state_end_pos);
        if (status)
            std::cout << "3. Almost equal = true (with reeds-shepp fallback)" << std::endl;
        
        return true;
    }

    if (use_reed_shepps_fallback || force_reeds_shepp_shot)
        if (state_counter % try_reeds_shepp_interval == 0 || force_reeds_shepp_shot) {
            force_reeds_shepp_shot = false;
            bool status = try_finish_with_reeds_shepp(cur_cell.pos, state_end_pos);
            if (status) {
                std::cout << "4. Reeds-shepp shot succeeded" << std::endl;
                return true;
            }
        }
    
    for (int dir = -1; dir <= 1; dir += 2)
        for (int steer = -1; steer <= 1; steer++) {
            motion_simulation_time.start();
            // auto motion_simulation_start = std::chrono::steady_clock::now();
            std::vector<NonholonomicPos> motion = NonholonomicAStar::simulate_motion(cur_cell.pos, steer, dir);
            motion_simulation_time.end();
            // auto motion_simulation_end = std::chrono::steady_clock::now();

            // motion_simulation_time.add(motion_simulation_end - motion_simulation_start);




            std::vector<NonholonomicPos> simplified_motion = {motion[0], motion[motion.size() - 1]};

            auto adjust_to_ground_start = std::chrono::steady_clock::now();
            if (!grid->adjust_to_ground(simplified_motion, max_step_up, max_drop, max_y_diff))
                continue;
            auto adjust_to_ground_end = std::chrono::steady_clock::now();

            adjust_to_ground_time.add(adjust_to_ground_end - adjust_to_ground_start);


            // auto get_ground_positions_start = std::chrono::steady_clock::now();
            get_ground_positions_time.start();
            std::vector<glm::ivec3> ground_positions;
            if (!grid->get_ground_positions(simplified_motion, ground_positions, max_step_up, max_drop, max_y_diff))
                continue;
            get_ground_positions_time.end();
            // auto get_ground_positions_end = std::chrono::steady_clock::now();

            // get_ground_positions_time.add(get_ground_positions_end - get_ground_positions_start);
            
            crosses_extreme_curvature_time.start();
            if (!crosses_extreme_curvature(ground_positions, curvature_limit))
                continue;
            crosses_extreme_curvature_time.end();

            float motion_dist = 0;
            for (int i = 0; i < motion.size() -1; i++) {
                motion_dist += glm::distance(motion[i].pos, motion[i+1].pos);
            }
            
            // NonholonomicPos new_pos = motion[motion.size() - 1];
            NonholonomicPos new_pos = simplified_motion[simplified_motion.size() - 1];

            uint64_t new_key = state_key(new_pos);
            auto heap_it = state_closed_heap.find(new_key);
            if (heap_it != state_closed_heap.end()) 
                continue;
            
            float new_g = cur_cell.g + motion_dist;

            auto it = state_g_score.find(new_key);    
            if (it != state_g_score.end()) {
                float old_g = it->second;
                if (old_g <= new_g)
                    continue;
            }
            
            state_g_score[new_key] = new_g;

            NonholonomicAStarCell new_cell;
            new_cell.pos = new_pos;
            new_cell.pos.steer = steer;
            new_cell.pos.dir = dir;
            new_cell.came_from = cur_cell.pos;
            new_cell.no_parent = false;
            new_cell.g = new_g;
            get_nonholonomic_f_time.start();
            new_cell.f = new_g + get_nonholonomic_f(new_cell.pos, state_end_pos, cur_cell.pos, state_plain_astar_path);
            get_nonholonomic_f_time.end();

            state_pq.push(new_cell);
        }
        state_counter++;
    return false;
}

void NonholonomicAStar::find_nonholomic_path() {
    while (true) {
        if (find_nonholomic_path_step())
            break;
    }
}