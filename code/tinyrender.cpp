#include "tinyrender.h"

#include <assert.h>			// assert
#include <stdio.h>      // sscanf, fprintf

#include "../dependency/imgui/backends/imgui_impl_glfw.h"
#include "../dependency/imgui/backends/imgui_impl_opengl3.h"

namespace tinyrender
{
	struct object_internal
	{
	public:
		GLuint vao = 0;
		GLuint buffers = 0;
		GLuint triangleBuffer = 0;
		float modelMatrix[4][4] = { 0 };
		int triangleCount = 0;

		bool isDeleted = false;
	};

	struct scene_internal
	{
	public:
		float zNear = 0.1f, zFar = 500.0f;
		v3f eye = { 10, 0, 0 };
		v3f at = { 0 };
		v3f up = { 0, 1, 0 };
		float camSpeed = 0.01f;
		v3f lightDir = { 1, 1, 0 };

		float deltaTime = 0.0f;
		float lastFrame = 0.0f;

		float mouseScrollingSpeed = 2.0f;
		float mouseSensitivity = 0.1f;
		float mouseLastX = 0.0f;
		float mouseLastY = 0.0f;
	};

	static GLFWwindow* windowPtr;
	static int width_internal, height_internal;
	static std::vector<object_internal> internalObjects;
	static std::vector<GLuint> internalShaders;
	static scene_internal internalScene;


	/*!
	\brief Initialize a matrix to identity.
	\param Result matrix to initialize
	*/
	static void _internalIdentity(float Result[4][4])
	{
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
				Result[i][j] = 0.0f;
		}
		Result[0][0] = 1.0f;
		Result[1][1] = 1.0f;
		Result[2][2] = 1.0f;
		Result[3][3] = 1.0f;
	}

	/*!
	\brief Compute the look at matrix for the current internal camera.
	\param Result look at matrix.
	*/
	static void _internalCameraLookAt(float Result[4][4])
	{
		v3f const f = internalNormalize(internalScene.at - internalScene.eye);
		v3f const s = internalNormalize(internalCross(f, { 0, 1, 0 }));
		v3f const u = internalCross(s, f);

		Result[0][0] = s.x;
		Result[1][0] = s.y;
		Result[2][0] = s.z;
		Result[0][1] = u.x;
		Result[1][1] = u.y;
		Result[2][1] = u.z;
		Result[0][2] = -f.x;
		Result[1][2] = -f.y;
		Result[2][2] = -f.z;
		Result[3][0] = -internalDot(s, internalScene.eye);
		Result[3][1] = -internalDot(u, internalScene.eye);
		Result[3][2] = internalDot(f, internalScene.eye);
		Result[3][3] = 1.0f;
	}

	/*!
	\brief Compute the perspective matrix for the current internal camera.
	\param Result perspective matrix.
	*/
	static void _internalCameraPerspective(float Result[4][4])
	{
		float const tanHalfFovy = tan(radian(45.0f) / 2.0f);
		float const zNear = internalScene.zNear;
		float const zFar = internalScene.zFar;

		Result[0][0] = 1.0f / (((float)width_internal) / ((float)height_internal) * tanHalfFovy);
		Result[1][1] = 1.0f / (tanHalfFovy);
		Result[2][2] = -(zFar + zNear) / (zFar - zNear);
		Result[2][3] = -1.0f;
		Result[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
	}

	/*
	\brief Apply a translation to the camera, in camera space.
	\param x translation X
	\param y translation Y
	\param z translation Z
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
		}
		if (z != 0.0f)
		{
			v3f f = internalNormalize(internalScene.at - internalScene.eye);
			internalScene.eye += f * z;
		}
		if (xPlane != 0.0f)
		{
			v3f f = internalScene.at - internalScene.eye;
			v3f s = internalNormalize(internalCross(internalScene.up, f));

			internalScene.eye += s * xPlane;
			internalScene.at += s * xPlane;
		}
		if (yPlane != 0.0f)
		{
			v3f u = internalScene.up;

			internalScene.eye += u * yPlane;
			internalScene.at += u * yPlane;
		}
	}

	/*
	\brief Compute the model matrices for a given position and scale. Rotation
	is not yet supported.
	\param Result the resulting model matrix
	\param p translation
	\param s scale
	*/
	static void _internalComputeModelMatrix(float Result[4][4], const v3f& p, const v3f& s)
	{
		// Identity
		_internalIdentity(Result);

		// Translation
		Result[3][0] = p.x;
		Result[3][1] = p.y;
		Result[3][2] = p.z;

		// No rotation for now
		// Empty

		// Scale
		Result[0][0] = s.x;
		Result[1][1] = s.y;
		Result[2][2] = s.z;
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
	\brief Create the internal description of an object. Initialize opengl buffers.
	\param obj high level descriptor of the object.
	*/
	static object_internal _internalCreateObject(const object& obj)
	{
		object_internal ret;

		// Model matrix
		_internalComputeModelMatrix(ret.modelMatrix, obj.position, obj.scale);

		// VAO
		glGenVertexArrays(1, &ret.vao);
		glBindVertexArray(ret.vao);

		// OpenGL buffers
		int fullSize = sizeof(v3f) * int(obj.vertices.size() + obj.normals.size() + obj.colors.size());
		glGenBuffers(1, &ret.buffers);
		glBindBuffer(GL_ARRAY_BUFFER, ret.buffers);
		glBufferData(GL_ARRAY_BUFFER, fullSize, nullptr, GL_STATIC_DRAW);
		int size = 0;
		int offset = 0;
		size = sizeof(v3f) * obj.vertices.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &obj.vertices.front());
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void*)offset);
		glEnableVertexAttribArray(0);
		offset = offset + size;
		size = sizeof(v3f) * obj.normals.size();
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, &obj.normals.front());
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const void*)offset);
		glEnableVertexAttribArray(1);
		if (obj.colors.size() > 0)
		{
			offset = offset + size;
			size = sizeof(v3f) * obj.colors.size();
			glBufferSubData(GL_ARRAY_BUFFER, offset, size, &obj.colors.front());
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const void*)offset);
			glEnableVertexAttribArray(2);
		}
		glGenBuffers(1, &ret.triangleBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret.triangleBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * obj.triangles.size(), &obj.triangles.front(), GL_STATIC_DRAW);
		ret.triangleCount = int(obj.triangles.size());

		return ret;
	}

	/*
	\brief Update an already created object with new vertice/normal data.
	\param id object index
	\param newObj new data for the object.
	*/
	static void _internalUpdateObject(int id, const object& newObj)
	{
		object_internal& obj = internalObjects[id];

		// Model matrix
		_internalComputeModelMatrix(obj.modelMatrix, newObj.position, newObj.scale);

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
	\brief Removes an object from the internal hierarchy.
	\param id object index
	*/
	static void _internalDeleteObject(int id)
	{
		object_internal& obj = internalObjects[id];
		if (obj.isDeleted)
			return;

		glDeleteBuffers(1, &obj.buffers);
		glDeleteBuffers(1, &obj.triangleBuffer);
		glDeleteVertexArrays(1, &obj.vao);

		// obj is not actually removed from the internal vector, but flagged as deleted.
		// This is to ensure indices of existing object will not change.
		obj.isDeleted = true;
	}

	/*
	\brief
	*/
	static int _internalGetNextFreeIndex()
	{
		for (int i = 0; i < internalObjects.size(); i++)
		{
			if (internalObjects[i].isDeleted)
				return i;
		}
		return internalObjects.size();
	}


	/*!
	\brief Init a window sized (width, height).
	*/
	void init(int width, int height)
	{
		// Window
		// -------------------------------------------------------------------------------
		width_internal = width;
		height_internal = height;
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
		windowPtr = glfwCreateWindow(width, height, "tinyrender", NULL, NULL);
		if (windowPtr == NULL)
		{
			fprintf(stderr, "Error creating window");
			glfwTerminate();
			return;
		}
		glfwMakeContextCurrent(windowPtr);
		glfwSetInputMode(windowPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetWindowSizeCallback(windowPtr, [](GLFWwindow* win, int w, int h)
			{
				glViewport(0, 0, w, h);
				width_internal = w;
				height_internal = h;
			});
		glfwSetScrollCallback(windowPtr, [](GLFWwindow* win, double x, double y)
			{
				_internalCameraMove(0.0f, 0.0f, y * internalScene.mouseScrollingSpeed, 0.0f, 0.0f);
			});

		// OpenGL
		// -------------------------------------------------------------------------------
		glewInit();
		glEnable(GL_DEPTH_TEST);
		GLenum err = glGetError();
		if (err != GL_NO_ERROR)
		{
			fprintf(stderr, "Error initializing opengl");
			glfwTerminate();
			return;
		}

		// Shaders
		// -------------------------------------------------------------------------------
		const GLchar* vertexShaderSource =
			"#version 330\n"
			"layout (location = 0) in vec3 vertex;\n"
			"layout (location = 1) in vec3 normal;\n"
			"layout (location = 2) in vec3 color;\n"
			"uniform mat4 u_projection;\n"
			"uniform mat4 u_view;\n"
			"uniform mat4 u_model;\n"
			"out vec3 fragPos;\n"
			"out vec3 fragNormal;\n"
			"out vec3 fragColor;\n"
			"void main()\n"
			"{\n"
			"	 fragPos = vertex;\n"
			"    gl_Position = u_projection * u_view * u_model * vec4(vertex, 1.0f);\n"
			"	 fragNormal = normalize(normal);\n"
			"    fragColor = color;\n"
			"}\n";
		const GLchar* fragmentShaderSource =
			"#version 330\n"
			"in vec3 fragPos;\n"
			"in vec3 fragNormal;\n"
			"in vec3 fragColor;\n"
			"uniform vec3 u_light_dir;\n"
			"out vec4 outFragmentColor;\n"
			"void main()\n"
			"{\n"
			"	 float d = 0.5 * (1.0 + dot(fragNormal, u_light_dir));"
			"	 outFragmentColor = vec4(fragColor * d, 1.0); \n"
			"}\n";
		GLuint g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
		g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(g_VertHandle, 1, &vertexShaderSource, NULL);
		glCompileShader(g_VertHandle);
		_internalCheckShader(g_VertHandle, "vertex shader");

		g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(g_FragHandle, 1, &fragmentShaderSource, NULL);
		glCompileShader(g_FragHandle);
		_internalCheckShader(g_FragHandle, "fragment shader");

		g_ShaderHandle = glCreateProgram();
		glAttachShader(g_ShaderHandle, g_VertHandle);
		glAttachShader(g_ShaderHandle, g_FragHandle);
		glLinkProgram(g_ShaderHandle);
		internalShaders.push_back(g_ShaderHandle);

		// Reserve some space for future object
		internalObjects.reserve(100);

		// Imgui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOpenGL(windowPtr, true);
		ImGui_ImplOpenGL3_Init("#version 330");
	}

	/*!
	\brief Returns true of the user closed the window, or pressed escape.
	Returns false otherwise.
	*/
	bool shouldQuit()
	{
		return glfwWindowShouldClose(windowPtr) || glfwGetKey(windowPtr, GLFW_KEY_ESCAPE);
	}

	/*
	\brief Check if a given key has been pressed in the last frame.
	\param key the key
	*/
	bool getKey(int key)
	{
		return bool(glfwGetKey(windowPtr, key));
	}

	/*!
	\brief
	*/
	float deltaTime()
	{
		return internalScene.deltaTime;
	}

	/*!
	\brief
	*/
	float globalTime()
	{
		return float(glfwGetTime());
	}

	/*
	\brief Update function. Applies internal orbiter camera translation
	and rotation.
	*/
	void update()
	{
		float currentFrame = float(glfwGetTime());
		internalScene.deltaTime = currentFrame - internalScene.lastFrame;
		internalScene.lastFrame = currentFrame;

		// Keyboard
		float x = 0.0f, y = 0.0f, z = 0.0f, xPlane = 0.0f, yPlane = 0.0f;
		if (glfwGetKey(windowPtr, GLFW_KEY_LEFT))
			x -= 0.1f;
		if (glfwGetKey(windowPtr, GLFW_KEY_RIGHT))
			x += 0.1f;
		if (glfwGetKey(windowPtr, GLFW_KEY_UP))
			y += 0.1f;
		if (glfwGetKey(windowPtr, GLFW_KEY_DOWN))
			y -= 0.1f;
		if (glfwGetKey(windowPtr, GLFW_KEY_PAGE_UP))
			z += 0.1f;
		if (glfwGetKey(windowPtr, GLFW_KEY_PAGE_DOWN))
			z -= 0.1f;

		// Mouse
		double xpos, ypos;
		glfwGetCursorPos(windowPtr, &xpos, &ypos);
		int state = glfwGetMouseButton(windowPtr, GLFW_MOUSE_BUTTON_LEFT);
		if (state == GLFW_PRESS)
		{
			float xoffset = float(xpos) - internalScene.mouseLastX;

			// Reversed since y-coordinates go from bottom to top
			float yoffset = internalScene.mouseLastY - float(ypos);
			x += (xoffset * internalScene.mouseSensitivity);
			y += (yoffset * internalScene.mouseSensitivity);
		}
		state = glfwGetMouseButton(windowPtr, GLFW_MOUSE_BUTTON_MIDDLE);
		if (state == GLFW_PRESS)
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
		z *= scale * internalScene.camSpeed * 0.0025f;
		xPlane *= scale * internalScene.camSpeed;
		yPlane *= scale * internalScene.camSpeed;

		// Apply everything
		_internalCameraMove(x, y, z, xPlane, yPlane);

		// Store last mouse pos
		internalScene.mouseLastX = float(xpos);
		internalScene.mouseLastY = float(ypos);
	}

	/*!
	\brief Performs rendering and swap window buffers.
	*/
	void render()
	{
		// Clear
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Camera matrices
		float viewMatrix[4][4] = { 0 }, projectionMatrix[4][4] = { 0 };
		_internalCameraLookAt(viewMatrix);
		_internalCameraPerspective(projectionMatrix);

		// Render all objects
		for (int i = 0; i < internalObjects.size(); i++)
		{
			object_internal& it = internalObjects[i];
			if (it.isDeleted)
				continue;

			// Always use the shader 0 for now.
			GLuint shaderID = internalShaders[0];

			glUseProgram(shaderID);
			glUniformMatrix4fv(glGetUniformLocation(shaderID, "u_projection"), 1, GL_FALSE, &projectionMatrix[0][0]);
			glUniformMatrix4fv(glGetUniformLocation(shaderID, "u_view"), 1, GL_FALSE, &viewMatrix[0][0]);
			glUniformMatrix4fv(glGetUniformLocation(shaderID, "u_model"), 1, GL_FALSE, &it.modelMatrix[0][0]);
			glUniform3f(glGetUniformLocation(shaderID, "u_light_dir"), 1, GL_FALSE, internalScene.lightDir[0]);

			glBindVertexArray(it.vao);
			glDrawElements(GL_TRIANGLES, it.triangleCount, GL_UNSIGNED_INT, 0);
		}

		// Prepare imgui frame for later
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	/*!
	\brief TODO
	*/
	void swap()
	{
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(windowPtr);
		glfwPollEvents();
	}

	/*!
	\brief Free allocated data and terminate window context.
	*/
	void terminate()
	{
		for (int i = 0; i < internalObjects.size(); i++)
			_internalDeleteObject(i);
		internalObjects.clear();
		glfwTerminate();
	}


	/*!
	\brief Add an object to the internal hierarchy. Rendering
	buffers are allocated here.
	\param object new object
	\returns the id of the object in the hierarchy.
	*/
	int pushObject(const object& obj)
	{
		object_internal internalObject = _internalCreateObject(obj);
		int index = _internalGetNextFreeIndex();
		if (index == internalObjects.size())
			internalObjects.push_back(internalObject);
		else
			internalObjects[index] = internalObject;
		return index;
	}

	/*!
	\brief Update an object with new data, given its id.
	Note that the internal object (with id as an identifier) should
	already be initialized.
	The update can also only be performed with data of same size; it will
	fail if you change the internal array sizes.
	\param id identifier
	\param obj new object data
	\returns true of update is successfull, false otherwise.
	*/
	void updateObject(int id, const object& obj)
	{
		assert(id < internalObjects.size());
		_internalUpdateObject(id, obj);
	}

	/*!
	\brief Update an object with a new model matrix, given its id.
	The internal object whould already be initialized.
	\param id identifier
	\param pos new position
	\param scale new scale
	*/
	void updateObject(int id, const v3f& position, const v3f& scale)
	{
		assert(id < internalObjects.size());
		object_internal& obj = internalObjects[id];
		_internalComputeModelMatrix(obj.modelMatrix, position, scale);
	}

	/*!
	\brief Removes an object from the internal hierarchy, given its id.
	\param id identifier
	\returns true of removal is successfull false otherwise.
	*/
	void popObject(int id)
	{
		assert(id < internalObjects.size());
		_internalDeleteObject(id);
	}

	/*
	\brief
	*/
	void popFirst()
	{
		for (int i = 0; i < internalObjects.size(); i++)
		{
			if (internalObjects[i].isDeleted == false)
			{
				popObject(i);
				return;
			}
		}
	}

	
	/*!
	\brief
	*/
	int pushPlaneRegularMesh(float size, int n)
	{
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

		return pushObject(planeObject);
	}

	/*!
	\brief
	*/
	void pushPlaneDenseMesh(float size, int n)
	{

	}


	/*!
	\brief Set the camera eye position.
	\param x
	\param y
	\param z
	*/
	void setCameraEye(float x, float y, float z)
	{
		internalScene.eye.x = x;
		internalScene.eye.y = y;
		internalScene.eye.z = z;
	}

	/*!
	\brief Set the camera look-at point.
	\param x
	\param y
	\param z
	*/
	void setCameraAt(float x, float y, float z)
	{
		internalScene.at.x = x;
		internalScene.at.y = y;
		internalScene.at.z = z;
	}

	/*
	\brief Set the camera planes.
	\param near near clipping plane
	\param far far clipping plane
	*/
	void setCameraPlanes(float near, float far)
	{
		internalScene.zNear = near;
		internalScene.zFar = far;
	}

	/*!
	\brief Set the light position.
	\param x
	\param y
	\param z
	*/
	void setLightDir(float x, float y, float z)
	{
		internalScene.lightDir.x = x;
		internalScene.lightDir.y = y;
		internalScene.lightDir.z = z;
	}
}
