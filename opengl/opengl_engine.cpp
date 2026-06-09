#include "opengl_engine.h"
#include "window.h"
#include "camera.h"

OpenGLEngine::OpenGLEngine(){
    init();

    mesh_manager = new MeshManager();
    material_manager = new MaterialManager();
}

OpenGLEngine::~OpenGLEngine(){
    glfwTerminate();
}

int OpenGLEngine::init() {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return -1;
    }

    // Request OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // Для дебага

    return 1;
}

int OpenGLEngine::init_glew() {
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewErr) << "\n";
        return -1;
    }
    glGetError();

    mesh_manager->load_cube();
    material_manager->load_blinn_phong();

    shader_manager = new ShaderManager(executable_dir_str(), std::vector<std::filesystem::path>());
    

    // default_vertex_shader = new VertexShader(default_vertex_shader_path);
    // default_fragment_shader = new FragmentShader(default_fragment_shader_path);
    default_program = new VfProgram(&shader_manager->default_vertex_shader, &shader_manager->default_fragment_shader);
    default_line_program = new VfProgram(&shader_manager->default_line_vertex_shader, &shader_manager->default_line_fragment_shader);
    default_point_program = new VfProgram(&shader_manager->default_point_vertex_shader, &shader_manager->default_point_fragment_shader);
    default_circle_program = new VfProgram(&shader_manager->default_cirlce_vertex_shader, &shader_manager->default_fragment_shader);
    skybox_program = new VfProgram(&shader_manager->skybox_vs, &shader_manager->skybox_fs);
    equirect_to_cubemap_program = new VfProgram(&shader_manager->equirect_to_cubemap_vs, &shader_manager->equirect_to_cubemap_fs);
    irradiance_program = new VfProgram(&shader_manager->irradiance_vs, &shader_manager->irradiance_fs);
    prefilter_program = new VfProgram(&shader_manager->prefilter_vs, &shader_manager->prefilter_fs);
    brdf_lut_program = new VfProgram(&shader_manager->brdf_lut_vs, &shader_manager->brdf_lut_fs);

    lighting_system.init(*shader_manager);


    texture_manager = new TextureManager(*this, executable_dir_str());
    return 1;
}

void OpenGLEngine::set_window(Window* window) {
    if (!window || !window->window) {
        std::cerr << "set_window: GLFWwindow is null (create failed)\n";
        return;
    }

    // window = window;

    glfwMakeContextCurrent(window->window);

    if (init_glew() != 1) {
        std::cerr << "OpenGL init failed\n";
        return;
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    enable_depth_test();

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                      GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_TRUE);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}


void OpenGLEngine::set_vulkan_window(VulkanWindow* window) {
    if (!window || !window->window) {
        std::cerr << "set_window: GLFWwindow is null (create failed)\n";
        return;
    }

    this->window = window;

    glfwMakeContextCurrent(window->window);

    if (init_glew() != 1) {
        std::cerr << "OpenGL init failed\n";
        return;
    }
    
    glEnable(GL_FRAMEBUFFER_SRGB);

    enable_depth_test();

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                      GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_TRUE);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}

void OpenGLEngine::enable_depth_test() {
    glEnable(GL_DEPTH_TEST);
}

void OpenGLEngine::poll_events() {
    glfwPollEvents();
}

void OpenGLEngine::begin_frame(const glm::vec4& clear_color) {
    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLEngine::end_frame() {
    if (!window) {
        return;
    }

    glfwSwapBuffers(window->window);
}

void OpenGLEngine::on_framebuffer_resized(int w, int h) {
    glViewport(0, 0, w, h);
}