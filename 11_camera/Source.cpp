#include <array>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <math.h>
#include <string>
#include <vector>

using namespace std;

extern void cleanUpScene();

GLFWwindow	*window;

// normal keys are fom [0..255], arrow and special keys are from [256..511]
GLboolean	keyboard[512] = {GL_FALSE};

int		window_width = 600;
int		window_height = 600;
char	window_title[] = "Surfaces";

std::vector<glm::vec3> pointsToDraw;
std::vector<glm::vec3> controlPoints;

GLint dragged = -1;

/* Vertex buffer objektum és vertex array objektum az adattároláshoz.*/
#define numVBOs	1
#define numVAOs	1
GLuint			VBO[numVBOs];
GLuint			VAO[numVAOs];

GLuint			renderingProgram;

unsigned int	modelLoc;
unsigned int	viewLoc;
unsigned int	projectionLoc;
int x, z;
GLuint pointNum, selectedPoint, drawingPoints;


/** Vetítési és kamera mátrixok felvétele. */
glm::mat4		view, projection = glm::perspective(glm::radians(45.0f), (float)window_width / (float)window_height, 0.1f, 100.0f);

GLdouble		currentTime, deltaTime, lastTime = 0.0f;
GLfloat			cameraSpeed;
glm::mat4		model		= glm::mat4(1.0f),
				translateM	= glm::mat4(1.0f);

/* Vegyük fel a kamera pozicót tároló változót, illetve a tengelyekhezz szükséges vektorokat. */
glm::vec3		cameraPos		= glm::vec3(0.2f, 0.3f,  0.8f), 
				cameraTarget	= glm::vec3(0.2f, 0.0f,  0.2f), 
				cameraUpVector	= glm::vec3(0.0f, 1.0f,  0.0f),
				cameraDirection	= glm::vec3(0.0f, 0.0f, -1.0f), // direction for camera
				cameraMovingY = glm::vec3(0.0f, 1.0f, 0.0f);

bool checkOpenGLError() {
	bool foundError = false;
	int glErr = glGetError();
	while (glErr != GL_NO_ERROR) {
		cout << "glError: " << glErr << endl;
		foundError = true;
		glErr = glGetError();
	}
	return foundError;
}

void printShaderLog(GLuint shader) {
	int len = 0;
	int chWrittn = 0;
	char* log;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
	if (len > 0) {
		log = (char*)malloc(len);
		glGetShaderInfoLog(shader, len, &chWrittn, log);
		cout << "Shader Info Log: " << log << endl;
		free(log);
	}
}

void printProgramLog(int prog) {
	int len = 0;
	int chWrittn = 0;
	char* log;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
	if (len > 0) {
		log = (char*)malloc(len);
		glGetProgramInfoLog(prog, len, &chWrittn, log);
		cout << "Program Info Log: " << log << endl;
		free(log);
	}
}

string readShaderSource(const char* filePath) {
	string content;
	ifstream fileStream(filePath, ios::in);
	string line = "";

	while (!fileStream.eof()) {
		getline(fileStream, line);
		content.append(line + "\n");
	}
	fileStream.close();
	return content;
}

GLuint createShaderProgram() {

	GLint vertCompiled;
	GLint fragCompiled;
	GLint linked;

	string vertShaderStr = readShaderSource("vertexShader.glsl");
	string fragShaderStr = readShaderSource("fragmentShader.glsl");

	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

	const char* vertShaderSrc = vertShaderStr.c_str();
	const char* fragShaderSrc = fragShaderStr.c_str();

	glShaderSource(vShader, 1, &vertShaderSrc, NULL);
	glShaderSource(fShader, 1, &fragShaderSrc, NULL);

	glCompileShader(vShader);
	checkOpenGLError();
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &vertCompiled);
	if (vertCompiled != 1) {
		cout << "vertex compilation failed" << endl;
		printShaderLog(vShader);
	}


	glCompileShader(fShader);
	checkOpenGLError();
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &fragCompiled);
	if (fragCompiled != 1) {
		cout << "fragment compilation failed" << endl;
		printShaderLog(fShader);
	}

	// Shader program objektum létrehozása. Eltároljuk az ID értéket.
	GLuint vfProgram = glCreateProgram();
	glAttachShader(vfProgram, vShader);
	glAttachShader(vfProgram, fShader);

	glLinkProgram(vfProgram);
	checkOpenGLError();
	glGetProgramiv(vfProgram, GL_LINK_STATUS, &linked);
	if (linked != 1) {
		cout << "linking failed" << endl;
		printProgramLog(vfProgram);
	}

	glDeleteShader(vShader);
	glDeleteShader(fShader);

	return vfProgram;
}

GLfloat dist2(glm::vec3 P1, glm::vec3 P2) {
	GLfloat dx = P1.x - P2.x;
	GLfloat dy = P1.y - P2.y;

	return dx * dx + dy * dy;
}

GLint getActivePoint(vector<glm::vec3> p, GLfloat sensitivity, GLfloat x, GLfloat y) {
	GLfloat		s = sensitivity * sensitivity;
	GLint		size = p.size();
	GLfloat		xNorm = x / (window_width / 2) - 1.0f;
	GLfloat		yNorm = y / (window_height / 2) - 1.0f;
	glm::vec3	P = glm::vec3(xNorm, yNorm, 0.0f);

	for (GLint i = size - 1; i >= 0; i--) {
		glm::vec4 projectPoint = projection * view * model * glm::vec4(p[i].x, p[i].y, p[i].z, 1.0);
	    glm::vec3 point = glm::vec3(projectPoint.x/projectPoint.w, projectPoint.y/projectPoint.w,0.0f);
		if (dist2(point, P) < s)
			return i;
	}
		return -1;
}


void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if ((action == GLFW_PRESS) && (key == GLFW_KEY_ESCAPE))
		cleanUpScene();

	if (action == GLFW_PRESS)
		keyboard[key] = GL_TRUE;
	else if (action == GLFW_RELEASE)
		keyboard[key] = GL_FALSE;
}

void cursorPosCallback(GLFWwindow* window, double xPos, double yPos)
{

}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		double	x, y;

		glfwGetCursorPos(window, &x, &y);
		dragged = getActivePoint(controlPoints, 0.05f, x, window_height - y);
	}
	cout << dragged << "\n";

}

/** Kiszámoljuk a kamera mátrixokat. */
void computeCameraMatrix() {
	/* A paraméterek rendre: az új koordinátarendszerünk középpontja (hol a kamera), merre néz a kamera, mit tekintünk ,,fölfele" iránynak */
	//view = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUpVector);
	view = glm::lookAt(cameraPos, cameraTarget, cameraUpVector);
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void generateControllPoints() {

	do {
		cout << "Pontok X mentén (min 2 max 10): ";
		cin >> x;

		cout << "Pontok Z mentén (min 2 max 10): ";
		cin >> z;
	} while (x <= 1 || z <= 1 || x > 10 || z > 10);
	

	float x_cord = 0.1f, z_cord = 0.1f, increment = 0.1f;

	for (int i = 0; i < x; i++)
	{
		for (int j = 0; j < z; j++)
		{
			controlPoints.push_back(glm::vec3(x_cord, 0.0f, z_cord));
			z_cord += increment;
		}
		x_cord += increment;
		z_cord = 0.1f;
	}

	glm::vec3 first = controlPoints[0];
	glm::vec3 last = controlPoints.back();

	cameraTarget = glm::vec3((first.x + last.x) / 2, 0.0f, (first.z + last.z) / 2);
	cameraPos.x = cameraTarget.x;
}

void generatePointsToDraw() {
	pointsToDraw.clear();
	for (int i = 0; i < controlPoints.size(); i++)
	{
		pointsToDraw.push_back(controlPoints[i]);
	}

	int index;
	for (int i = 0; i < z; i++)
	{
		index = i;
		for (int j = 0; j < x; j++)
		{
			pointsToDraw.push_back(controlPoints[index]);
			index += z;
		}
	}
}

glm::vec3 rotatePoint(glm::vec3 center, float angle, glm::vec3 point) {
	float s = sin(angle);
	float c = cos(angle);

	point.x -= center.x;
	point.z -= center.z;

	float new_x = point.x * c - point.z * s;
	float new_z = point.x * s + point.z * c;

	point.x = new_x + center.x;
	point.z = new_z + center.z;

	return point;
}

void init(GLFWwindow* window) {
	renderingProgram = createShaderProgram();
	generateControllPoints();
	generatePointsToDraw();

	/* Létrehozzuk a szükséges Vertex buffer és vertex array objektumot. */
	glGenBuffers(numVBOs, VBO);
	glGenVertexArrays(numVAOs, VAO);

	/* Típus meghatározása: a GL_ARRAY_BUFFER nevesített csatolóponthoz kapcsoljuk a buffert (ide kerülnek a vertex adatok). */
	glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);

	/* Másoljuk az adatokat a pufferbe! Megadjuk az aktuálisan csatolt puffert,  azt hogy hány bájt adatot másolunk,
	a másolandó adatot, majd a feldolgozás módját is meghatározzuk: most az adat nem változik a feltöltés után */
	glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);

	/* Csatoljuk a vertex array objektumunkat a konfiguráláshoz. */
	glBindVertexArray(VAO[0]);

	/* Ezen adatok szolgálják a 0 indexû vertex attribútumot (itt: pozíció).
	Elsõként megadjuk ezt az azonosítószámot.
	Utána az attribútum méretét (vec3, láttuk a shaderben).
	Harmadik az adat típusa.
	Negyedik az adat normalizálása, ez maradhat FALSE jelen példában.
	Az attribútum értékek hogyan következnek egymás után? Milyen lépésköz után találom a következõ vertex adatait?
	Végül megadom azt, hogy honnan kezdõdnek az értékek a pufferben. Most rögtön, a legelejétõl veszem õket.*/
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	/* Engedélyezzük az imént definiált 0 indexû attribútumot. */
	glEnableVertexAttribArray(0);

	/* Szín attribútum */

	/* Leválasztjuk a vertex array objektumot és a puffert is.*/
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// aktiváljuk a shader-program objektumunkat.
	// activate the shader program
	glUseProgram(renderingProgram);

	// get the location of the vertex variables
	modelLoc = glGetUniformLocation(renderingProgram, "model");
	viewLoc = glGetUniformLocation(renderingProgram, "view");
	projectionLoc = glGetUniformLocation(renderingProgram, "projection");

	pointNum = glGetUniformLocation(renderingProgram, "pointNum");
	selectedPoint = glGetUniformLocation(renderingProgram, "selectedPoint");
	drawingPoints = glGetUniformLocation(renderingProgram, "drawingPoints");
	// set the projection, since it change rarely
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
	// set black for clearing
	glClearColor(0.0, 0.0, 0.0, 1.0);
}

/** A jelenetünk utáni takarítás. */
void cleanUpScene() {
	/* töröljük a GLFW ablakot. */
	glfwDestroyWindow(window);

	/** Töröljük a vertex puffer és vertex array objektumokat. */
	glDeleteVertexArrays(numVAOs, VAO);
	glDeleteBuffers(numVBOs, VBO);

	/** Töröljük a shader programot. */
	glDeleteProgram(renderingProgram);

	/* Leállítjuk a GLFW-t */
	// stop GLFW
	glfwTerminate();

	exit(EXIT_SUCCESS);
}

void display() {
	glClear(GL_COLOR_BUFFER_BIT); // fontos lehet minden egyes alkalommal törölni!

	currentTime	= glfwGetTime();
	deltaTime	= currentTime - lastTime;
	lastTime	= currentTime;
	//deltaTime	=	glfwGetTime() - lastTime;
	//lastTime	+=	deltaTime;

	// this makes the animation continous, because camera speed proportional with elapsed time
	cameraSpeed = 2.5f * (GLfloat)deltaTime;
	// we will use two set ups for moving: WASD and the arrow keys
	if (keyboard[GLFW_KEY_UP]) {
		if (cameraPos.y > 0.08f) {
			cameraPos += cameraSpeed * glm::normalize((cameraTarget - cameraPos));
		}
	}

	if (keyboard[GLFW_KEY_DOWN]) {
		if (cameraPos.y < 1.5f) {
			cameraPos -= cameraSpeed * glm::normalize((cameraTarget - cameraPos));
		}		
	}

	if (keyboard[GLFW_KEY_RIGHT]) {
		cameraPos = rotatePoint(cameraTarget, -0.01f, cameraPos);
	}

	if (keyboard[GLFW_KEY_LEFT]) {
		cameraPos = rotatePoint(cameraTarget, 0.01f, cameraPos);
	}

	if (keyboard[GLFW_KEY_W]) {
		cameraPos += cameraSpeed * cameraMovingY;
	}

	if (keyboard[GLFW_KEY_S]) {
		cameraPos -= cameraSpeed * cameraMovingY;
	}

	if ((keyboard[GLFW_KEY_F]))
		if (dragged != -1) {
			controlPoints[dragged].y += 0.001;
			generatePointsToDraw();
			glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
			glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	if ((keyboard[GLFW_KEY_L]))
		if (dragged != -1) {
			controlPoints[dragged].y -= 0.001;
			generatePointsToDraw();
			glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
			glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		

	//translateM = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	//model = translateM;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	computeCameraMatrix();

	glProgramUniform1f(renderingProgram, pointNum, controlPoints.size());
	glProgramUniform1f(renderingProgram, selectedPoint, dragged);
	glProgramUniform1f(renderingProgram, drawingPoints, 1.0f);

	/*Csatoljuk a vertex array objektumunkat. */
	glBindVertexArray(VAO[0]);
	glPointSize(10);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDrawArrays(GL_POINTS, 0, pointsToDraw.size()/2);

	glProgramUniform1f(renderingProgram, drawingPoints, 2.0f);

	size_t begin = 0;
	for (size_t i = 0; i <x; i++)
	{
		glDrawArrays(GL_LINE_STRIP, begin, z);
		begin += z;
	}

	begin = controlPoints.size();
	for (size_t i = 0; i < z; i++)
	{
		glDrawArrays(GL_LINE_STRIP, begin, x);
		begin += x;
	}
	/* Leválasztjuk, nehogy bármilyen érték felülíródjon.*/
	glBindVertexArray(0);	
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	window_width = width;
	window_height = height;

	glViewport(0, 0, width, height);

	projection = glm::perspective(glm::radians(45.0f), (float)window_width / (float)window_height, 0.1f, 100.0f);
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

int main(void) {
	/* Próbáljuk meg inicializálni a GLFW-t! */
	if (!glfwInit()) { exit(EXIT_FAILURE); }

	/* A kívánt OpenGL verzió (4.3) */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	/* Próbáljuk meg létrehozni az ablakunkat. */
	window = glfwCreateWindow(window_width, window_height, window_title, nullptr, nullptr);

	/* Válasszuk ki az ablakunk OpenGL kontextusát, hogy használhassuk. */
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	/* Incializáljuk a GLEW-t, hogy elérhetõvé váljanak az OpenGL függvények. */
	if (glewInit() != GLEW_OK) { exit(EXIT_FAILURE); }
	glfwSwapInterval(1);

	/* Az alkalmazáshoz kapcsolódó elõkészítõ lépések, pl. hozd létre a shader objektumokat. */
	init(window);

	while (!glfwWindowShouldClose(window)) {
		/* a kód, amellyel rajzolni tudunk a GLFWwindow ojektumunkba. */
		display();
		/* double buffered */
		glfwSwapBuffers(window);
		/* események kezelése az ablakunkkal kapcsolatban, pl. gombnyomás */
		glfwPollEvents();
	}

	cleanUpScene();
	
	return EXIT_SUCCESS;
}