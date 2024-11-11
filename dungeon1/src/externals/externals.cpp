#include <externals.h>
#include <game.h>
#include <stdio.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <corecrt_malloc.h>

#include <glad.h>

#include <GLFW/glfw3.h>
#include <printLog.h>

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

static void glfw_error_callback(int error, const char *description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EXPORT int init_externals(game *g) {
	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	const char *glsl_version = "#version 450";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	g->window = glfwCreateWindow(1920, 1080, "Hello World", NULL, NULL);
	if (!g->window) {
		fprintf(stderr, "Failed to create GLFW window\n");
		glfwTerminate();
		return -1;
	}

	int monitorCount;
	GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

	GLFWmonitor *monitor = monitors[3];
	int monitorX, monitorY;
	glfwGetMonitorPos(monitor, &monitorX, &monitorY);

	const GLFWvidmode *vidMode = glfwGetVideoMode(monitor);
	if (vidMode == NULL) {
		fprintf(stderr, "Failed to get video mode for monitor #4\n");
		glfwDestroyWindow(g->window);
		glfwTerminate();
		return -1;
	}

	printf("monitor widht %i, height %i\n", vidMode->width, vidMode->height);
	if (vidMode->width == 2560 && vidMode->height == 1600) {
		printf(
			"this is my normal setup so I'll position the window manually\n");
		int windowWidth = 1920;
		int windowHeight = 1080;
		int xpos = monitorX /*+ (vidMode->width - windowWidth)*/;
		int ypos = monitorY + 30;
		glfwSetWindowPos(g->window, xpos, ypos);
	}

	glfwMakeContextCurrent(g->window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGuiContext *ctx = ImGui::CreateContext();
	if (!ctx) {
		fprintf(stderr, "Failed to create ImGui context\n");
		glfwDestroyWindow(g->window);
		glfwTerminate();
		return -1;
	}

	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();
	ImGui::SetCurrentContext(ctx);

	if (!ImGui_ImplGlfw_InitForOpenGL(g->window, true)) {
		fprintf(stderr, "Failed to initialize ImGui_ImplGlfw\n");
		ImGui::DestroyContext(ctx);
		glfwDestroyWindow(g->window);
		glfwTerminate();
		return -1;
	}

	if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
		fprintf(stderr, "Failed to initialize ImGui_ImplOpenGL3\n");
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext(ctx);
		glfwDestroyWindow(g->window);
		glfwTerminate();
		return -1;
	}

	g->loader = (GLADloadproc)glfwGetProcAddress;
	if (!gladLoadGLLoader((GLADloadproc)g->loader)) {
		printf("Failed to initialize GLAD in DLL\n");
		return -1;
	}
	g->play = true;
	g->ctx = ctx;
	ImGui::GetAllocatorFunctions(&g->alloc_func, &g->free_func, &g->user_data);

	return 1;
}

EXPORT void update_externals(game *g) {

	glfwMakeContextCurrent(g->window);

	glfwPollEvents();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	g->g_imguiUpdate(g);

	ImGui::SetCurrentContext(g->ctx);
	ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);

	ImGui::Render();
	int display_w, display_h;
	glfwGetFramebufferSize(g->window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(g->window);

	g->play = !glfwWindowShouldClose(g->window);
}

EXPORT void end_externals(game *g) {

	if (g->ctx) {
		ImGui::SetCurrentContext(g->ctx);

		// Shut down ImGui for GLFW and OpenGL
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		// Destroy ImGui context
		ImGui::DestroyContext(g->ctx);
		g->ctx = nullptr;
	}

	if (g->window) {
		glfwDestroyWindow(g->window);
		g->window = nullptr;
	}

	glfwTerminate();

	// Log that externals have been successfully shut down
	print_log("Externals have been successfully shut down", COLOR_YELLOW);
}

EXPORT ImGuiContext *GetImguiContext() { return ImGui::GetCurrentContext(); }
