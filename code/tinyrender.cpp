#include "tinyrender.h"

#include <assert.h>		// assert
#include <stdio.h>      // fprintf
#include <fstream>		// ostream, endl
#include <string>		// std::to_string

#include "../dependency/imgui/backends/imgui_impl_glfw.h"
#include "../dependency/imgui/backends/imgui_impl_opengl3.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../dependency/tinyobj/tiny_obj_loader.h"

namespace tinyrender
{
	struct object_internal
	{
	public:
		GLuint vao = 0;
		GLuint buffers = 0;
		GLuint triangleBuffer = 0;
		m4 modelMatrix;
		int triangleCount = 0;
		bool isDeleted = false;
	};

	struct scene_internal
	{
	public:
		// Camera settings
		float zNear = 0.1f, zFar = 500.0f;
		v3f eye = { 10, 0, 0 };
		v3f at = { 0 };
		v3f up = { 0, 1, 0 };
		float camSpeed = 0.01f;
		m4 bakedLookAtMatrix;
		m4 bakedPerspectiveMatrix;
		bool cameraWasChangedInLastframe = true;

		// Interactions
		float mouseScrollingSpeed = 2.0f;
		float mouseSensitivity = 0.1f;
		float mouseLastX = 0.0f;
		float mouseLastY = 0.0f;
		bool isMouseOverGui = false;
		int selectedObjectIndex = -1;
		int currentMouseButton = -1;

		// Frame timers
		float deltaTime = 0.0f;
		float lastFrame = 0.0f;

		// Gizmo
		ImGuizmo::OPERATION guizmoOp = ImGuizmo::TRANSLATE;

		// Render flags
		v3f lightDir = { 1, 1, 0 };
		bool doLighting = true;
		bool showNormals = false;
		bool drawWireframe = true;
		float wireframeThickness = 1.0f;
	};

	static GLFWwindow* windowPtr;
	static int width_internal, height_internal;
	static std::vector<object_internal> internalObjects;
	static std::vector<GLuint> internalShaders;
	static scene_internal internalScene;
	static bool initWasCalled = false;


	/*
	\brief Apply a translation to the camera, in camera space. Also deals with panning in screen space.
	\param x, y, z translation in camera space
	\param xPlane, yPlane translation in screen space (panning)
	*/
	static void _internalCameraMove(float x, float y, float z, float xPlane, float yPlane)
	{
		if (x != 0.0f)
		{
			v3f f = internalScene.at - internalScene.eye;
			v3f s = internalCross(internalScene.up, f);
			f = v3f({ f.x * cos(x) - f.z * sin(x), f.y, f.x * sin(x) + f.z * cos(x) });
			s = v3f({ s.x * cos(x) - s.z * sin(x), 0.0, s.x * sin(x) + s.z * cos(x) });

			internalScene.up = internalNormalize(internalCross(s, -f));
			internalScene.eye = internalScene.at - f;

			internalScene.cameraWasChangedInLastframe = true;
		}
		if (y != 0.0f)
		{
			v3f f = internalScene.at - internalScene.eye;
			float length = internalLength(f);
			f /= length;

			v3f s = internalNormalize(internalCross(internalScene.up, f));

			f = f * cos(y) + internalScene.up * sin(y);

			internalScene.up = internalCross(f, s);
			internalScene.eye = internalScene.at - f * length;

			internalScene.cameraWasChangedInLastframe = true;
		}
		if (z != 0.0f)
		{
			v3f f = internalScene.at - internalScene.eye;
			float moveScale = internalLength(f) * 0.025f;
			internalScene.eye += (internalNormalize(f) * z) * moveScale;

			internalScene.cameraWasChangedInLastframe = true;
		}
		if (xPlane != 0.0f)
		{
			v3f f = internalScene.at - internalScene.eye;
			v3f s = internalNormalize(internalCross(internalScene.up, f));

			internalScene.eye += s * xPlane;
			internalScene.at += s * xPlane;
			
			internalScene.cameraWasChangedInLastframe = true;
		}
		if (yPlane != 0.0f)
		{
			v3f u = internalScene.up;

			internalScene.eye += u * yPlane;
			internalScene.at += u * yPlane;
			
			internalScene.cameraWasChangedInLastframe = true;
		}
	}

	/*!
	\brief Check the shader compile status and print opengl logs if needed.
	\param handle shader id
	\param desc shader descriptor string, such as "Vertex Lit", "Fragment Lit" etc...
	*/
	static bool _internalCheckShader(GLuint handle, const char* desc)
	{
		GLint status = 0, log_length = 0;
		glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
		if ((GLboolean)status == GL_FALSE)
			fprintf(stderr, "ERROR: Could not compile shader: failed to compile %s!\n", desc);
		if (log_length > 1)
		{
			char* buf = new char[int(log_length + 1)];
			glGetShaderInfoLog(handle, log_length, NULL, (GLchar*)buf);
			fprintf(stderr, "%s\n", buf);
			delete[] buf;
		}
		return (GLboolean)status == GL_TRUE;
	}

	/*!
	\brief Create the internal representation of an object. Initialize opengl buffers.
	\param obj high level object with mesh, color data, and model matrix.
	*/
	static object_internal _internalCreateObject(const object& obj)
	{
		object_internal ret;

		// Default colors
		std::vector<v3f> colors = obj.colors;
		if (colors.empty())
			colors.resize(obj.vertices.size(), { 0.5f, 0.5f, 0.5f });

		// Model matrix
		ImGuizmo::RecomposeMatrixFromComponents(&obj.position.x, &obj.rotation.x, &obj.scale.x, ret.modelMatrix.m);

		// VAO
		glGenVertexArrays(1, &ret.vao);
		glBindVertexArray(ret.vao);

		// OpenGL buffers
		size_t fullSize = sizeof(v3f) * size_t(obj.vertices.size() + obj.normals.size() + colors.size());
		glGenBuffers(1, &ret.buffers);
		glBindBuffer(GL_ARRAY_BUFFER, ret.buffers);
		glBufferData(GL_ARRAY_BUFFER, fullSize, nullptr, GL_STATIC_DRAW);

		size_t size = 0;
		size_t offset = 0;
		size = sizeof(v3f) * obj.vertices.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &obj.vertices.front());
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void*)offset);
		glEnableVertexAttribArray(0);

		offset = offset + size;
		size = sizeof(v3f) * obj.normals.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &obj.normals.front());
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const void*)offset);
		glEnableVertexAttribArray(1);

		offset = offset + size;
		size = sizeof(v3f) * colors.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &colors.front());
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const void*)offset);
		glEnableVertexAttribArray(2);

		glGenBuffers(1, &ret.triangleBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret.triangleBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * obj.triangles.size(), &obj.triangles.front(), GL_STATIC_DRAW);
		ret.triangleCount = int(obj.triangles.size());

		return ret;
	}

	/*
	\brief Update an already created object with new vertice/normal (and optionally color) data.
	Also recompute the model matrix.
	\param id object index
	\param newObj new data for the object.
	*/
	static void _internalUpdateObject(int id, const object& newObj)
	{
		object_internal& obj = internalObjects[id];

		// Model matrix
		ImGuizmo::RecomposeMatrixFromComponents(&newObj.position.x, &newObj.rotation.x, &newObj.scale.x, obj.modelMatrix.m);

		glBindVertexArray(obj.vao);
		glBindBuffer(GL_ARRAY_BUFFER, obj.buffers);
		size_t size = 0;
		size_t offset = 0;
		size = sizeof(v3f) * newObj.vertices.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &newObj.vertices.front());
		offset = offset + size;
		size = sizeof(v3f) * newObj.normals.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &newObj.normals.front());
		if (newObj.colors.size() != 0)
		{
			offset = offset + size;
			size = sizeof(v3f) * newObj.colors.size();
			glBufferSubData(GL_ARRAY_BUFFER, offset, size, &newObj.colors.front());
		}
	}

	/*
	\brief Update an already created object with new color data.
	\param id object index
	\param newColors new color data for the object.
	*/
	static void _internalUpdateObject(int id, const std::vector<v3f>& newColors)
	{
		assert(!newColors.empty());
		object_internal& obj = internalObjects[id];
		glBindVertexArray(obj.vao);
		glBindBuffer(GL_ARRAY_BUFFER, obj.buffers);
		size_t size = 0;
		size_t offset = 0;
		size = sizeof(v3f) * newColors.size();
		offset = offset + 2 * size; // offset = vertexCount + normalCount (and vertexCount == colorCount)
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &newColors.front());
	}

	/*
	\brief Deletes an object from the internal hierarchy. Destroys the opengl buffers.
	\param index object index
	*/
	static bool _internalDeleteObject(int index)
	{
		object_internal& obj = internalObjects[index];
		if (obj.isDeleted)
			return false;

		glDeleteBuffers(1, &obj.buffers);
		glDeleteBuffers(1, &obj.triangleBuffer);
		glDeleteVertexArrays(1, &obj.vao);

		// Objects are not actually removed from the internal vector, but flagged as deleted.
		// This is to ensure indices of existing objects will not change from the API point of view.
		obj.isDeleted = true;

		return true;
	}

	/*!
	\brief Try to delete an object from the scene given its id.
	Will not throw any error if the id is invalid or no object exists with this id.
	\param id the object id
	*/
	static bool _internalTryDeleteObject(int id)
	{
		if (id < 0 || id >= internalObjects.size())
			return false;
		for (int i = 0; i < internalObjects.size(); i++)
		{
			if (i == id)
				return _internalDeleteObject(i);
		}
		return false;
	}

	/*
	\brief Returns the next free index in the internal object array.
	*/
	static int _internalGetNextFreeIndex()
	{
		for (int i = 0; i < internalObjects.size(); i++)
		{
			if (internalObjects[i].isDeleted)
				return i;
		}
		return int(internalObjects.size());
	}

	/*!
	\brief Internal GUI rendering with dear imgui.
	*/
	static void _internalRenderGui()
	{
		ImGui::Begin("tinyrender");
		{
			ImGui::Text("Rendering");
			{
				if (ImGui::Checkbox("Lighting", &internalScene.doLighting))
					setDoLighting(internalScene.doLighting);
				if (ImGui::Checkbox("Wireframe", &internalScene.drawWireframe))
					setDrawWireframe(internalScene.drawWireframe);
				if (ImGui::SliderFloat("Thickness", &internalScene.wireframeThickness, 1.0f, 2.0f))
					setWireframeThickness(internalScene.wireframeThickness);
				if (ImGui::Checkbox("Show Normals", &internalScene.showNormals))
					setShowNormals(internalScene.showNormals);
				ImGui::Text("Light direction");
				ImGui::DragFloat("x", &internalScene.lightDir.x, 0.1f, -1.0f, 1.0f);
				ImGui::DragFloat("y", &internalScene.lightDir.y, 0.1f, -1.0f, 1.0f);
				ImGui::DragFloat("z", &internalScene.lightDir.z, 0.1f, -1.0f, 1.0f);
				ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / float(ImGui::GetIO().Framerate), float(ImGui::GetIO().Framerate));
			}

			ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Camera");
			{	
				ImGui::Text("Eye (%.3f, %.3f, %.3f)", internalScene.eye.x, internalScene.eye.y, internalScene.eye.z);
				ImGui::Text("At (%.3f, %.3f, %.3f)", internalScene.at.x, internalScene.at.y, internalScene.at.z);
				ImGui::Text("Up (%.3f, %.3f, %.3f)", internalScene.up.x, internalScene.up.y, internalScene.up.z);
			}

			ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Scene");
			{
				for (int i = 0; i < internalObjects.size(); i++)
				{
					if (internalObjects[i].isDeleted)
						continue;

					bool selected = (internalScene.selectedObjectIndex == i);
					std::string objectName = "Object " + std::to_string(i);
					if (ImGui::Selectable(objectName.c_str(), selected))
						internalScene.selectedObjectIndex = i;
				}
			}
		}
		ImGui::End();
	}

	/*!
	\brief Callback for mouse button events.
	\param window glfw window pointer
	\param w new window width
	\param h new window height
	*/
	static void _internalWindowResizeCallback(GLFWwindow* window, int w, int h)
	{
		glViewport(0, 0, w, h);
		width_internal = w;
		height_internal = h;
		internalScene.cameraWasChangedInLastframe = true;
	}

	/*!
	\brief Callback for mouse button events. See https://www.glfw.org/docs/3.3/input_guide.html#input_mouse_button
	for more information.
	\param window glfw window pointer
	\param button the modified button
	\param action GLFW_PRESS or GLFW_RELEASE
	\param mods current modifiers pressed
	*/
	static void _internalMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		if (action == GLFW_PRESS)
		{
			internalScene.currentMouseButton = button;
		}
		else if (action == GLFW_RELEASE)
		{
			internalScene.currentMouseButton = -1;
			if (!internalScene.isMouseOverGui && !ImGuizmo::IsUsing())
				internalScene.selectedObjectIndex = -1;
		}
	}

	/*!
	\brief Callback for mouse scrolling. See https://www.glfw.org/docs/3.3/input_guide.html#scrolling
	for more information.
	\param window glfw window pointer
	\param x, y scroll motion. Only the y parameter is relevant as scroll is vertical.
	*/
	static void _internalScrollCallback(GLFWwindow* window, double x, double y)
	{
		_internalCameraMove(0.0f, 0.0f, float(y) * internalScene.mouseScrollingSpeed, 0.0f, 0.0f);
	}

	/*!
	\brief Callback for key events. See https://www.glfw.org/docs/3.3/input_guide.html#input_key
	for more information.
	\param window glfw window pointer
	\param key the modified key
	\param scancode platform specific scancode.
	\param action GLFW_PRESS, GLFW_REPEAT or GLFW_RELEASE.
	\param mods current modifiers pressed
	*/
	static void _internalKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if (action == GLFW_PRESS)
		{
			float x = 0.0f, y = 0.0f, z = 0.0f;

			// Scene management
			if (key == GLFW_KEY_DELETE)
			{
				bool success = _internalTryDeleteObject(internalScene.selectedObjectIndex);
				if (success)
					internalScene.selectedObjectIndex = -1;
			}
			// Guizmo shortcut
			else if (key == GLFW_KEY_T)
				internalScene.guizmoOp = ImGuizmo::TRANSLATE;
			else if (key == GLFW_KEY_R)
				internalScene.guizmoOp = ImGuizmo::ROTATE;
			else if (getKey(GLFW_KEY_S))
				internalScene.guizmoOp = ImGuizmo::SCALE;
			// Keyboard	
			else if (key == GLFW_KEY_LEFT)
				x -= 0.1f;
			else if (key == GLFW_KEY_RIGHT)
				x += 0.1f;
			else if (key == GLFW_KEY_UP)
				y += 0.1f;
			else if (key == GLFW_KEY_DOWN)
				y -= 0.1f;
			else if (key == GLFW_KEY_PAGE_UP)
				z += 0.1f;
			else if (key == GLFW_KEY_PAGE_DOWN)
				z -= 0.1f;

			if (!internalScene.isMouseOverGui && !ImGuizmo::IsUsing())
				_internalCameraMove(x, y, z, 0.0f, 0.0f); // No panning here
		}
	}


	/*!
	\brief Init a window sized (width, height) with a given name.
	\param windowName name of the window on top bar
	\param width, height window dimensions. If either is -1, then the window will
	be maximized at startup with screen resolution.
	*/
	void init(const char* windowName, int width, int height)
	{
		// Window
		glfwInit();
		if (width == -1 || height == -1)
		{
			const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
			width = mode->width;
			height = mode->height;
		}
		width_internal = width;
		height_internal = height;

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
		glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
		windowPtr = glfwCreateWindow(width, height, windowName, NULL, NULL);
		if (windowPtr == NULL)
		{
			fprintf(stderr, "Error creating GLFW window - terminating");
			glfwTerminate();
			return;
		}
		glfwMakeContextCurrent(windowPtr);
		glfwShowWindow(windowPtr);
		glfwSwapInterval(1);

		// Inputs and callbacks
		glfwSetInputMode(windowPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetWindowSizeCallback(windowPtr, _internalWindowResizeCallback);
		glfwSetScrollCallback(windowPtr, _internalScrollCallback);
		glfwSetKeyCallback(windowPtr, _internalKeyCallback);
		glfwSetMouseButtonCallback(windowPtr, _internalMouseButtonCallback);

		// OpenGL
		glewInit();
		glEnable(GL_DEPTH_TEST);
		GLenum err = glGetError();
		if (err != GL_NO_ERROR)
		{
			fprintf(stderr, "Error initializing opengl - terminating");
			glfwTerminate();
			return;
		}

		// Shaders
		const GLchar* vertexShaderSource =
			"#version 330\n"
			"layout (location = 0) in vec3 vertex;\n"
			"layout (location = 1) in vec3 normal;\n"
			"layout (location = 2) in vec3 color;\n"
			"uniform mat4 uProjection;\n"
			"uniform mat4 uView;\n"
			"uniform mat4 uModel;\n"
			"out vec3 geomPos;\n"
			"out vec3 geomNormal;\n"
			"out vec3 geomColor;\n"
			"void main()\n"
			"{\n"
			"	 geomPos = vertex;\n"
			"    gl_Position = uProjection * uView * uModel * vec4(vertex, 1.0f);\n"
			"	 geomNormal = normalize(normal);\n"
			"    geomColor = color;\n"
			"}\n";
		const GLchar* geometryShaderSource =
			"#version 330\n"
			"layout(triangles) in;\n"
			"layout(triangle_strip, max_vertices = 3) out;\n"
			"in vec3 geomPos[];\n"
			"in vec3 geomNormal[];\n"
			"in vec3 geomColor[];\n"
			"uniform vec2 uWireframeThickness;\n"
			"out vec3 fragPos;\n"
			"out vec3 fragNormal;\n"
			"out vec3 fragColor;\n"
			"out vec3 dist;\n"
			"void main()\n"
			"{\n"
			"	vec2 p0 = uWireframeThickness * gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;\n"
			"	vec2 p1 = uWireframeThickness * gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;\n"
			"	vec2 p2 = uWireframeThickness * gl_in[2].gl_Position.xy / gl_in[2].gl_Position.w;\n"
			"	vec2 v0 = p2 - p1;\n"
			"	vec2 v1 = p2 - p0;;\n"
			"	vec2 v2 = p1 - p0;;\n"
			"	float area = abs(v1.x*v2.y - v1.y * v2.x);\n"
			"	dist = vec3(area / length(v0), 0, 0);\n"
			"	gl_Position = gl_in[0].gl_Position;\n"
			"	fragPos = geomPos[0]; fragColor = geomColor[0];  fragNormal = geomNormal[0];\n"
			"	EmitVertex();\n"
			"	dist = vec3(0, area / length(v1), 0);\n"
			"	gl_Position = gl_in[1].gl_Position;\n"
			"	fragPos = geomPos[1]; fragColor = geomColor[1];  fragNormal = geomNormal[1];\n"
			"	EmitVertex();\n"
			"	dist = vec3(0, 0, area / length(v2));\n"
			"	gl_Position = gl_in[2].gl_Position;\n"
			"	fragPos = geomPos[2]; fragColor = geomColor[2];  fragNormal = geomNormal[2];\n"
			"	EmitVertex();\n"
			"	EndPrimitive();\n"
			"}\n";
		const GLchar* fragmentShaderSource =
			"#version 330\n"
			"in vec3 fragPos;\n"
			"in vec3 fragNormal;\n"
			"in vec3 fragColor;\n"
			"in vec3 dist;\n"
			"uniform vec3 uLightDir;\n"
			"uniform int uDoLighting;\n"
			"uniform int uDrawWireframe;\n"
			"uniform int uShowNormals;\n"
			"out vec4 outFragmentColor;\n"
			"void main()\n"
			"{\n"
			"	 float d = uDoLighting == 1 ? 0.5 * (1.0 + dot(fragNormal, uLightDir)) : 1.0f;\n"
			"	 vec3 col = fragColor;\n"
			"	 if (uShowNormals == 1) {\n"
			"		col = vec3(0.2*(vec3(3.0,3.0,3.0)+2.0*fragNormal));\n"
			"		d = 1.f\n;"
			"	 }\n"
			"	 float w = min(dist[0], min(dist[1], dist[2]));\n"
			"	 float I = exp2(-1 * w * w);\n"
			"	 if (uDrawWireframe == 1)\n"
			"		col = I * vec3(0.1) + (1.0 - I) * col;\n"
			"	 outFragmentColor = vec4(col * d, 1.0); \n"
			"}\n";
		GLuint g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0, g_GeomHandle = 0;
		g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(g_VertHandle, 1, &vertexShaderSource, NULL);
		glCompileShader(g_VertHandle);
		if (!_internalCheckShader(g_VertHandle, "vertex shader"))
		{
			fprintf(stderr, "Error initializing vertex shader - terminating");
			return;
		}

		g_GeomHandle = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(g_GeomHandle, 1, &geometryShaderSource, NULL);
		glCompileShader(g_GeomHandle);
		if (!_internalCheckShader(g_GeomHandle, "geometry shader"))
		{
			fprintf(stderr, "Error initializing geometry shader - terminating");
			return;
		}

		g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(g_FragHandle, 1, &fragmentShaderSource, NULL);
		glCompileShader(g_FragHandle);
		if (!_internalCheckShader(g_FragHandle, "fragment shader"))
		{
			fprintf(stderr, "Error initializing fragment shader - terminating");
			return;
		}

		g_ShaderHandle = glCreateProgram();
		glAttachShader(g_ShaderHandle, g_VertHandle);
		glAttachShader(g_ShaderHandle, g_GeomHandle);
		glAttachShader(g_ShaderHandle, g_FragHandle);
		glLinkProgram(g_ShaderHandle);
		internalShaders.push_back(g_ShaderHandle);

		// Imgui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOpenGL(windowPtr, true);
		ImGui_ImplOpenGL3_Init("#version 330");

		initWasCalled = true;
	}

	/*!
	\brief Returns true if the user closed the window or pressed escape, false otherwise.
	*/
	bool shouldQuit()
	{
		return glfwWindowShouldClose(windowPtr) || glfwGetKey(windowPtr, GLFW_KEY_ESCAPE);
	}

	/*
	\brief Check if a given key is being pressed.
	\param key the key
	*/
	bool getKey(int key)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");
		return bool(glfwGetKey(windowPtr, key));
	}

	/*!
	\brief Returns the current delta time between two frames.
	*/
	float deltaTime()
	{
		return internalScene.deltaTime;
	}

	/*!
	\brief Returns the elapsed time since window initialization.
	*/
	float globalTime()
	{
		return float(glfwGetTime());
	}

	/*
	\brief Update function. Applies camera movements and guizmo updates.
	*/
	void update()
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		float currentFrame = float(glfwGetTime());
		internalScene.deltaTime = currentFrame - internalScene.lastFrame;
		internalScene.lastFrame = currentFrame;

		// Camera motion through mouse
		double xpos, ypos;
		glfwGetCursorPos(windowPtr, &xpos, &ypos);
		float x = 0.0f, y = 0.0f, z = 0.0f, xPlane = 0.0f, yPlane = 0.0f;
		if (internalScene.currentMouseButton == GLFW_MOUSE_BUTTON_LEFT)
		{
			float xoffset = float(xpos) - internalScene.mouseLastX;

			// Reversed since y-coordinates go from bottom to top
			float yoffset = internalScene.mouseLastY - float(ypos);
			x += (xoffset * internalScene.mouseSensitivity);
			y += (yoffset * internalScene.mouseSensitivity);
		}
		else if (internalScene.currentMouseButton == GLFW_MOUSE_BUTTON_MIDDLE)
		{
			float xoffset = float(xpos) - internalScene.mouseLastX;
			float yoffset = float(ypos) - internalScene.mouseLastY;
			xPlane += (xoffset * internalScene.mouseSensitivity);
			yPlane += (yoffset * internalScene.mouseSensitivity);
		}

		// Scale speed based on distance to the look at point
		float scale = internalLength(internalScene.at - internalScene.eye);
		scale = scale > 100.0f ? 100.0f : scale;
		x *= scale * internalScene.camSpeed * 0.25f;
		y *= scale * internalScene.camSpeed * 0.25f;
		z *= scale * internalScene.camSpeed * 0.025f;
		xPlane *= scale * internalScene.camSpeed;
		yPlane *= scale * internalScene.camSpeed;

		// Apply everything
		if (!internalScene.isMouseOverGui && !ImGuizmo::IsUsing())
			_internalCameraMove(x, y, z, xPlane, yPlane);

		// Store last mouse pos
		internalScene.mouseLastX = float(xpos);
		internalScene.mouseLastY = float(ypos);
	}

	/*!
	\brief Performs rendering: scene, imgui, and finally imguizmo.
	*/
	void render()
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		// Clear
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Camera matrices
		if (internalScene.cameraWasChangedInLastframe)
		{
			internalScene.bakedLookAtMatrix = lookAtMatrix(
				internalScene.eye,
				internalScene.at
			);
			internalScene.bakedPerspectiveMatrix = perspectiveMatrix(
				internalScene.zNear, 
				internalScene.zFar,
				float(width_internal),
				float(height_internal)
			);
			internalScene.cameraWasChangedInLastframe = false;
		}

		// Precomputed uniform values
		const float wireframeThicknessX = float(width_internal) / internalScene.wireframeThickness;
		const float wireframeThicknessY = float(height_internal) / internalScene.wireframeThickness;
		const v3f normalizedLight = internalNormalize(internalScene.lightDir);

		// Single shader for now
		GLuint shaderID = internalShaders[0];
		glUseProgram(shaderID);

		// Shared uniforms
		glUniformMatrix4fv(glGetUniformLocation(shaderID, "uProjection"), 1, GL_FALSE, internalScene.bakedPerspectiveMatrix.m);
		glUniformMatrix4fv(glGetUniformLocation(shaderID, "uView"), 1, GL_FALSE, internalScene.bakedLookAtMatrix.m);
		glUniform3f(glGetUniformLocation(shaderID, "uLightDir"), normalizedLight.x, normalizedLight.y, normalizedLight.z);
		glUniform1i(glGetUniformLocation(shaderID, "uDoLighting"), int(internalScene.doLighting));
		glUniform1i(glGetUniformLocation(shaderID, "uDrawWireframe"), int(internalScene.drawWireframe));
		glUniform2f(glGetUniformLocation(shaderID, "uWireframeThickness"), wireframeThicknessX, wireframeThicknessY);
		glUniform1i(glGetUniformLocation(shaderID, "uShowNormals"), int(internalScene.showNormals));

		// Render all objects
		for (int i = 0; i < internalObjects.size(); i++)
		{
			object_internal& it = internalObjects[i];
			if (it.isDeleted)
				continue;

			// Only unique uniform for an object is its model matrix
			glUniformMatrix4fv(glGetUniformLocation(shaderID, "uModel"), 1, GL_FALSE, it.modelMatrix.m);

			glBindVertexArray(it.vao);
			glDrawElements(GL_TRIANGLES, it.triangleCount, GL_UNSIGNED_INT, 0);
		}

		// Prepare imgui/imguizmo frames
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		internalScene.isMouseOverGui = ImGui::GetIO().WantCaptureMouse;
		
		// Guizmo
		ImGuizmo::BeginFrame();
		if (internalScene.selectedObjectIndex != -1)
		{
			ImGuizmo::Enable(true);
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetRect(0, 0, float(width_internal), float(height_internal));
			ImGuizmo::Manipulate(
				internalScene.bakedLookAtMatrix.m,
				internalScene.bakedPerspectiveMatrix.m,
				internalScene.guizmoOp,
				ImGuizmo::WORLD,
				internalObjects[internalScene.selectedObjectIndex].modelMatrix.m,
				NULL,
				NULL,
				NULL,
				NULL
			);
		}

		// Internal gui and guizmo rendering
		_internalRenderGui();
	}

	/*!
	\brief End the current frame. Must be called after update, render, and
	custom dear imgui calls.
	*/
	void swap()
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(windowPtr);
		glfwPollEvents();
	}

	/*!
	\brief Free all allocated data and terminate window context.
	*/
	void terminate()
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		for (int i = 0; i < internalObjects.size(); i++)
			_internalDeleteObject(i);
		internalObjects.clear();
		glfwTerminate();
	}


	/*!
	\brief Add an object to the internal hierarchy. The object must be initialized in a specific way.
	Vertex and normal arrays should be of the same size, and the triangle array refers to both vertex and normal indices.
	Colors are optionals and will be set to grey by default.
	\param object new object with properly initialized vectors.
	\return the id of the object in the hierarchy.
	*/
	int addObject(const object& obj)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		object_internal internalObject = _internalCreateObject(obj);
		int index = _internalGetNextFreeIndex();
		if (index == internalObjects.size())
			internalObjects.push_back(internalObject);
		else
			internalObjects[index] = internalObject;
		return index;
	}

	/*!
	\brief Removes an object from the internal hierarchy, given its id.
	\param id identifier
	\return true if removal was successful, false otherwise.
	*/
	bool removeObject(int id)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");
		assert(id >= 0 && id < internalObjects.size());
		return _internalDeleteObject(id);
	}

	/*!
	\brief Update an object with new data, given its id. Note that the internal object (with id as an identifier) should
	already be initialized.	The update must also only be be with data of same size; it will fail if you change the
	internal array sizes.
	\param id identifier
	\param obj new object data
	\return true of update is successful, false otherwise.
	*/
	void updateObject(int id, const object& obj)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");
		assert(id >= 0 && id < internalObjects.size());
		_internalUpdateObject(id, obj);
	}

	/*!
	\brief Update an object with a new position and scale, given its id. The internal object whould already be initialized.
	\param id identifier
	\param pos new position
	\param scale new scale
	*/
	void updateObject(int id, const v3f& position, const v3f& rotation, const v3f& scale)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");
		assert(id >= 0 && id < internalObjects.size());
		object_internal& obj = internalObjects[id];
		ImGuizmo::RecomposeMatrixFromComponents(&position.x, &rotation.x, &scale.x, obj.modelMatrix.m);
	}

	/*!
	\brief Update an object colors given its id.
	\param id object id
	\param newColors new per-vertex color array. Must be of the same size as the vertex array of the existing object.
	*/
	void updateObject(int id, const std::vector<v3f>& newColors)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");
		assert(id >= 0 && id < internalObjects.size());
		assert(!newColors.empty());
		_internalUpdateObject(id, newColors);
	}


	/*!
	\brief Set the lighting flag (ie. should diffuse light be computed or not)
	\param doLighting new flag
	*/
	void setDoLighting(bool doLighting)
	{
		internalScene.doLighting = doLighting;
	}

	/*!
	\brief Draw the wireframe of all the objects or not.
	\param drawWireframe new flag
	*/
	void setDrawWireframe(bool drawWireframe)
	{
		internalScene.drawWireframe = drawWireframe;
	}

	/*!
	\brief Set the wireframe line thicknes on the screen.
	Something between [1, 2] works well. Default is 1.
	\param thickness the new thickness value
	*/
	void setWireframeThickness(float thickness)
	{
		internalScene.wireframeThickness = thickness;
	}

	/*!
	\brief Override the current rendering state and show the normals of all objects.
	\param showNormals new flag
	*/
	void setShowNormals(bool showNormals)
	{
		internalScene.showNormals = showNormals;
	}

	/*!
	\brief Set the camera eye position.
	\param newEye new eye position
	*/
	void setCameraEye(const v3f& newEye)
	{
		internalScene.eye = newEye;
	}

	/*!
	\brief Set the camera look-at point.
	\param newAt new at position
	*/
	void setCameraAt(const v3f& newAt)
	{
		internalScene.at = newAt;
	}

	/*
	\brief Set the camera planes.
	\param near, far new near/far clipping plane
	*/
	void setCameraPlanes(float near, float far)
	{
		internalScene.zNear = near;
		internalScene.zFar = far;
	}

	/*!
	\brief Set the light direction (must be normalized).
	\param new light direction
	*/
	void setLightDir(const v3f& newLightDir)
	{
		internalScene.lightDir = newLightDir;
	}


	/*!
	\brief Add a new sphere object of a given radius centered at the origin.
	\param r radius
	\param n subdivision parameter
	\return the id of the new sphere object
	*/
	int addSphere(float r, int n)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		object newObj;

		const int p = 2 * n;
		const int s = (2 * n) * (n - 1) + 2;
		newObj.vertices.resize(s);
		newObj.normals.resize(s);

		// Create set of vertices
		const float Pi = 3.14159265358979323846f;
		const float HalfPi = Pi / 2.0f;
		const float dt = Pi / n;
		const float df = Pi / n;
		int k = 0;

		float f = -HalfPi;
		for (int j = 1; j < n; j++)
		{
			f += df;

			// Theta
			float t = 0.0;
			for (int i = 0; i < 2 * n; i++)
			{
				v3f u = { cos(t) * cos(f), sin(f), sin(t) * cos(f) };
				newObj.normals[k] = u;
				newObj.vertices[k] = u * r;
				k++;
				t += dt;
			}
		}
		// North pole
		newObj.normals[s - 2] = { 0, 1, 0 };
		newObj.vertices[s - 2] = { 0, r, 0 };

		// South
		newObj.normals[s - 1] = { 0, -1, 0 };
		newObj.vertices[s - 1] = { 0, -r, 0 };

		// Reserve space for the smooth triangle array
		newObj.triangles.reserve(4 * n * (n - 1) * 3);

		// South cap
		for (int i = 0; i < 2 * n; i++)
		{
			newObj.triangles.push_back(s - 1);
			newObj.triangles.push_back((i + 1) % p);
			newObj.triangles.push_back(i);
		}

		// North cap
		for (int i = 0; i < 2 * n; i++)
		{
			newObj.triangles.push_back(s - 2);
			newObj.triangles.push_back(2 * n * (n - 2) + i);
			newObj.triangles.push_back(2 * n * (n - 2) + (i + 1) % p);
		}

		// Sphere
		for (int j = 1; j < n - 1; j++)
		{
			for (int i = 0; i < 2 * n; i++)
			{
				const int v0 = (j - 1) * p + i;
				const int v1 = (j - 1) * p + (i + 1) % p;
				const int v2 = j * p + (i + 1) % p;
				const int v3 = j * p + i;

				newObj.triangles.push_back(v0);
				newObj.triangles.push_back(v1);
				newObj.triangles.push_back(v2);

				newObj.triangles.push_back(v0);
				newObj.triangles.push_back(v2);
				newObj.triangles.push_back(v3);
			}
		}

		return addObject(newObj);
	}

	/*!
	\brief Creates a subdivided plane object of a given size centered at the origin.
	\param size total extents of the plane
	\param n subdivision, ie. number of cells.
	\return the id of the new plane object
	*/
	int addPlane(float size, int n)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		n = n + 1;
		v3f a({ -size, 0.0f, -size });
		v3f b({ size, 0.0f, size });
		v3f step = (b - a) / float(n - 1);
		object planeObject;

		// Vertices
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < n; j++)
			{
				v3f v = a + v3f({ step.x * i, 0.f, step.z * j });
				planeObject.vertices.push_back(v);
				planeObject.normals.push_back({ 0.f, 1.f, 0.f });
				planeObject.colors.push_back({ 0.7f, 0.7f, 0.7f });
			}
		}

		// Triangles
		for (int i = 0; i < n - 1; i++)
		{
			for (int j = 0; j < n - 1; j++)
			{
				int v0 = (j * n) + i;
				int v1 = (j * n) + i + 1;
				int v2 = ((j + 1) * n) + i;
				int v3 = ((j + 1) * n) + i + 1;

				// tri 0
				planeObject.triangles.push_back(v0);
				planeObject.triangles.push_back(v1);
				planeObject.triangles.push_back(v2);

				// tri 1
				planeObject.triangles.push_back(v2);
				planeObject.triangles.push_back(v1);
				planeObject.triangles.push_back(v3);
			}
		}

		return addObject(planeObject);
	}

	/*!
	\brief Creates a cubic box object of a given size centered at the origin.
	The cube is actually made of 6 quads each with their own vertices and normals.
	\param size radius of the box
	\return the id of the new box object
	*/
	int addBox(float size)
	{
		assert(initWasCalled && "You must call tinyrender::init() before anything else");

		object newObj;

		const float r = size / 2.0f;

		// x negative
		newObj.vertices.push_back({ -r, -r, -r }); newObj.vertices.push_back({ -r, r, -r });
		newObj.vertices.push_back({ -r, r, r }); newObj.vertices.push_back({ -r, -r, r });
		newObj.normals.push_back({ -1, 0, 0 });	newObj.normals.push_back({ -1, 0, 0 });
		newObj.normals.push_back({ -1, 0, 0 });	newObj.normals.push_back({ -1, 0, 0 });
		newObj.triangles.push_back(0); newObj.triangles.push_back(1); newObj.triangles.push_back(2);
		newObj.triangles.push_back(0); newObj.triangles.push_back(2); newObj.triangles.push_back(3);

		// x positive
		newObj.vertices.push_back({ r, -r, -r }); newObj.vertices.push_back({ r, r, -r });
		newObj.vertices.push_back({ r, r, r }); newObj.vertices.push_back({ r, -r, r });
		newObj.normals.push_back({ 1, 0, 0 });	newObj.normals.push_back({ 1, 0, 0 });
		newObj.normals.push_back({ 1, 0, 0 });	newObj.normals.push_back({ 1, 0, 0 });
		newObj.triangles.push_back(4); newObj.triangles.push_back(5); newObj.triangles.push_back(6);
		newObj.triangles.push_back(4); newObj.triangles.push_back(6); newObj.triangles.push_back(7);

		// y negative
		newObj.vertices.push_back({ -r, -r, -r }); newObj.vertices.push_back({ -r, -r, r });
		newObj.vertices.push_back({ r, -r, r }); newObj.vertices.push_back({ r, -r, -r });
		newObj.normals.push_back({ 0, -1, 0 });	newObj.normals.push_back({ 0, -1, 0 });
		newObj.normals.push_back({ 0, -1, 0 });	newObj.normals.push_back({ 0, -1, 0 });
		newObj.triangles.push_back(8); newObj.triangles.push_back(9); newObj.triangles.push_back(10);
		newObj.triangles.push_back(8); newObj.triangles.push_back(10); newObj.triangles.push_back(11);

		// y positive
		newObj.vertices.push_back({ -r, r, -r }); newObj.vertices.push_back({ -r, r, r });
		newObj.vertices.push_back({ r, r, r }); newObj.vertices.push_back({ r, r, -r });
		newObj.normals.push_back({ 0, 1, 0 });	newObj.normals.push_back({ 0, 1, 0 });
		newObj.normals.push_back({ 0, 1, 0 });	newObj.normals.push_back({ 0, 1, 0 });
		newObj.triangles.push_back(12); newObj.triangles.push_back(13); newObj.triangles.push_back(14);
		newObj.triangles.push_back(12); newObj.triangles.push_back(14); newObj.triangles.push_back(15);

		// z negative
		newObj.vertices.push_back({ -r, -r, -r }); newObj.vertices.push_back({ -r, r, -r });
		newObj.vertices.push_back({ r, r, -r }); newObj.vertices.push_back({ r, -r, -r });
		newObj.normals.push_back({ 0, 0, -1 });	newObj.normals.push_back({ 0, 0, -1 });
		newObj.normals.push_back({ 0, 0, -1 });	newObj.normals.push_back({ 0, 0, -1 });
		newObj.triangles.push_back(16); newObj.triangles.push_back(17); newObj.triangles.push_back(18);
		newObj.triangles.push_back(16); newObj.triangles.push_back(18); newObj.triangles.push_back(19);

		// z positive
		newObj.vertices.push_back({ -r, -r, r }); newObj.vertices.push_back({ -r, r, r });
		newObj.vertices.push_back({ r, r, r }); newObj.vertices.push_back({ r, -r, r });
		newObj.normals.push_back({ 0, 0, 1 });	newObj.normals.push_back({ 0, 0, 1 });
		newObj.normals.push_back({ 0, 0, 1 });	newObj.normals.push_back({ 0, 0, 1 });
		newObj.triangles.push_back(20); newObj.triangles.push_back(21); newObj.triangles.push_back(22);
		newObj.triangles.push_back(20); newObj.triangles.push_back(22); newObj.triangles.push_back(23);

		return addObject(newObj);
	}

	/*!
	\brief Exports a given object as a .obj mesh file.
	\param filename filename to export
	\param object the object, which should be filled with contents (ie. vertices, triangles...)
	*/
	bool exportObjFile(const char* filename, const object& object)
	{
		std::ofstream out;
		out.open(filename);
		if (out.is_open() == false)
		{
			fprintf(stderr, "Could not open file for saving obj - terminating");
			return false;
		}
		out << "g " << "Obj" << std::endl;
		for (int i = 0; i < object.vertices.size(); i++)
			out << "v " << object.vertices.at(i).x << " " << object.vertices.at(i).y << " " << object.vertices.at(i).z << '\n';
		for (int i = 0; i < object.normals.size(); i++)
			out << "vn " << object.normals.at(i).x << " " << object.normals.at(i).z << " " << object.normals.at(i).y << '\n';
		for (int i = 0; i < object.triangles.size(); i += 3)
		{
			out << "f " << object.triangles.at(i) + 1 << "//" << object.triangles.at(i) + 1
				<< " " << object.triangles.at(i + 1) + 1 << "//" << object.triangles.at(i + 1) + 1
				<< " " << object.triangles.at(i + 2) + 1 << "//" << object.triangles.at(i + 2) + 1
				<< '\n';
		}
		out.close();
		return true;
	}

	/*!
	\brief Load a mesh .obj file. This function does not add the object
	to the scene. The object is guaranteed to have vertices, normals, and triangles.
	\param filename file to load
	\param object the returned object
	\return true of the loading was successfull, false otherwise.
	*/
	bool loadObjFile(const char* filename, object& obj)
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename);
		if (!err.empty() || !ret)
		{
			fprintf(stderr, "Could not load obj file - terminating");
			fprintf(stderr, err.c_str());
			return false;
		}

		// Load the raw obj
		obj.vertices.resize(attrib.vertices.size() / 3);
		obj.normals.resize(attrib.vertices.size() / 3);
		for (size_t s = 0; s < shapes.size(); s++) 
		{
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) 
			{
				int fv = shapes[s].mesh.num_face_vertices[f];
				for (int v = 0; v < fv; v++) 
				{
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
					obj.vertices[idx.vertex_index] =
					{
						attrib.vertices[3 * idx.vertex_index + 0],
						attrib.vertices[3 * idx.vertex_index + 1],
						attrib.vertices[3 * idx.vertex_index + 2]
					};
					if (idx.normal_index >= 0)
					{
						obj.normals[idx.vertex_index] = {
							attrib.normals[3 * idx.normal_index + 0],
							attrib.normals[3 * idx.normal_index + 1],
							attrib.normals[3 * idx.normal_index + 2]
						};
					}
					obj.triangles.push_back(idx.vertex_index);
				}
				index_offset += fv;
			}
		}

		// Make sure the loaded object has normals
		if (attrib.normals.size() == 0)
		{
			for (int i = 0; i < obj.triangles.size(); i += 3)
			{
				const auto& v0 = obj.vertices[obj.triangles[i + 0]];
				const auto& v1 = obj.vertices[obj.triangles[i + 1]];
				const auto& v2 = obj.vertices[obj.triangles[i + 2]];
				tinyrender::v3f n = tinyrender::internalCross((v1 - v0), (v2 - v0));
				obj.normals[obj.triangles[i + 0]] += n;
				obj.normals[obj.triangles[i + 1]] += n;
				obj.normals[obj.triangles[i + 2]] += n;
			}
			for (int i = 0; i < obj.normals.size(); i++)
				obj.normals[i] = tinyrender::internalNormalize(obj.normals[i]);
		}

		return true;
	}
}
