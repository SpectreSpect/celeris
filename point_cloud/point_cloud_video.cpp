#include "point_cloud_video.h"

static inline bool parse_u64(const std::string& s, uint64_t& out) {
    try { out = std::stoull(s); return true; } catch (...) { return false; }
}
static inline bool parse_u32(const std::string& s, uint32_t& out) {
    try { out = static_cast<uint32_t>(std::stoul(s)); return true; } catch (...) { return false; }
}
static inline bool parse_f(const std::string& s, float& out) {
    try { out = std::stof(s); return true; } catch (...) { return false; }
}

void PointCloudVideo::load_from_file(const std::filesystem::path& csv_path, int max_frames) {
    std::ifstream in(csv_path);
    if (!in) throw std::runtime_error("Failed to open: " + csv_path.string());

    std::vector<IndexEntry> entries;
    std::filesystem::path video_dir_path = csv_path.parent_path();
    std::string line;

    int loaded_count = 0;

    while (std::getline(in, line)) {
        if (loaded_count > max_frames)
            break;

        if (line.empty()) continue;

        std::vector<std::string> tok;
        {
            std::stringstream ss(line);
            std::string t;
            while (std::getline(ss, t, ',')) tok.push_back(t);
        }

        // Expect at least: id, t_ns, filename, count, px,py,pz, roll,pitch,yaw
        if (tok.size() < 10) continue;

        IndexEntry e;
        if (!parse_u64(tok[0], e.frame_id)) continue;
        if (!parse_u64(tok[1], e.timestamp_ns)) continue;
        e.filename = tok[2];
        if (!parse_u32(tok[3], e.point_count)) continue;

        float px, py, pz, rr, pp, yy;
        if (!parse_f(tok[4], px) || !parse_f(tok[5], py) || !parse_f(tok[6], pz)) continue;
        if (!parse_f(tok[7], rr) || !parse_f(tok[8], pp) || !parse_f(tok[9], yy)) continue;

        e.position = glm::vec3(px, py, pz);
        e.rotation_rpy = glm::vec3(rr, pp, yy);

        entries.push_back(std::move(e));

        loaded_count++;
    }

    std::sort(entries.begin(), entries.end(),
              [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });

    frames.clear();
    frames.reserve(entries.size());

    for (const auto& e : entries) {
        frames.emplace_back();
        frames.back().load_from_file(video_dir_path / e.filename);

        // Apply SAME basis remap as points:
        frames.back().point_cloud.position = ros_pos_to_engine(e.position);
        frames.back().point_cloud.rotation = ros_rpy_to_engine_rpy(e.rotation_rpy);
        frames.back().car_pos = ros_pos_to_engine(e.position);
        frames.back().car_rotation = ros_rpy_to_engine_rpy(e.rotation_rpy);
    }
}


size_t PointCloudVideo::get_frame_id(float time, size_t search_start_id) {
    int frame_id = 0;
    for (int i = search_start_id; i < frames.size(); i++) {
        double time_seconds = (double)frames[i].timestamp_ns / 1000000000.0f;

        if (time_seconds > timer)
            return frame_id;
        else
            frame_id = i;
    }

    return frame_id;
}

void PointCloudVideo::move(float time) {
    timer = time;
    current_frame = get_frame_id(timer);
}

void PointCloudVideo::pause() {
    paused = true;
}

void PointCloudVideo::resume() {
    paused = false;
}

void PointCloudVideo::update(float delta_time) {
    if (paused) return;

    if (looped && current_frame == frames.size() - 1) {
        current_frame = 0;
        timer = 0.0f;
    }

    timer += delta_time;
    current_frame = get_frame_id(timer, current_frame);
}

void PointCloudVideo::draw(RenderState state) {
    state.transform *= get_model_matrix();

    if (current_frame > frames.size())
        throw std::out_of_range("PointCloudVideo::draw");
    
    frames[current_frame].draw(state);

    // for (int i = 0; i <= current_frame; i++)
    //     frames[i].draw(state);

    // frames[0].draw(state);
    // frames[250].draw(state);


}