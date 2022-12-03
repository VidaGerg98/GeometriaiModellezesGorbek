/*
Jobbra-balra nyilakkal forgatás
Fel-le nyilakkal közelítés, távolodás
F: kiválasztott pont mozgatása felfele
L: kiválasztott pont mozgatása lefele
W: kamera mozgatása felfele
S: kamera mozgatása lefele
G: Grid ki-be kapcsolása
C: kontrollpontok ki-be kapcsolása
R: kontrollpontok alaphelyzetbe állítása
1: Bezier felület
2: B-spline felület
*/

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

#include <Windows.h>

using namespace std;

//bezierháló sûrûsége
int density = 5;

//bezier = 1
//b-spline = 2
int surface_id = 1;
int temp_X, temp_Z, temp_surface_id, temp_density;

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
GLuint pointNum, selectedPoint, drawingPoints, numberOfPointsToDraw;
GLfloat lineCount = 10.0f;
bool showGrid = true, showCPoints = true, controllPointsToDefault = true, numberOfControllPointsChanged = false, surfaceIdChange = false, densityChange = false;


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

int NCR(int n, int r) {
	if (r == 0) return 1;

	if (r > n / 2) return NCR(n, n - r);

	long res = 1;

	for (int k = 1; k <= r; ++k) {
		res *= n - k + 1;
		res /= k;
	}

	return res;
}

GLfloat BFunction(GLfloat i, GLfloat n, GLfloat u) {
	return NCR(n, i) * pow(u, i) * pow(1.0f - u, n - i);
}

glm::vec3 BezierPoints(GLfloat u, GLfloat v) {
	glm::vec3 point = glm::vec3(0.0f, 0.0f, 0.0f);
	GLint n = x - 1;
	GLint m = z - 1;

	for (int i = 0; i <= n; i++) {
		for (int j = 0; j <= m; j++) {
			GLfloat B1 = BFunction(i, n, u);
			GLfloat B2 = BFunction(j, m, v);
			point.x += B1 * B2 * controlPoints.at(i * z + j).x;
			point.y += B1 * B2 * controlPoints.at(i * z + j).y;
			point.z += B1 * B2 * controlPoints.at(i * z + j).z;
		}
	}

	return point;
}

GLfloat NFunction(int i, int p, GLfloat u, std::vector<float> vect) {
	GLfloat A, B, Asz, An, Bsz, Bn, NA, NB, res;

	if (p == 0) {
		if (vect[i] <= u && u < vect[i + 1]) {
			return 1.0f;
		}
		else {
			return 0.0f;
		}
	}

	NA = NFunction(i, p - 1, u, vect);
	NB = NFunction(i + 1, p - 1, u, vect);

	if (NA != 0) {
		An = (vect[i + p] - vect[i]);
		if (An != 0) {
			Asz = (u - vect[i]);
			A = (Asz / An) * NA;
		}
		else {
			A = 0;
		}
	}
	else {
		A = 0;
	}

	if (NB != 0) {
		Bn = (vect[i + p + 1] - vect[i + 1]);
		if (Bn != 0) {
			Bsz = (vect[i + p + 1] - u);
			B = (Bsz / Bn) * NB;
		}
		else {
			B = 0;
		}
	}
	else {
		B = 0;
	}
	
	res = A + B;

	return res;
}

glm::vec3 BSplinePoints(GLfloat u, GLfloat v, std::vector<GLfloat> U, std::vector<GLfloat> V) {
	glm::vec3 point = glm::vec3(0.0f, 0.0f, 0.0f);
	GLint n = z - 1;
	GLint m = x - 1;
	GLint p, q;
	if (m >= 3) {
		 p = 3;
	}
	else {
		p = 2;
	}
	if (n >= 3) {
		q = 3;
	}
	else {
		q = 2;
	}
	
	for (int i = 0; i <= m; i++) {
		for (int j = 0; j <= n; j++) {
			GLfloat N1 = NFunction(i, p, u, U);
			GLfloat N2 = NFunction(j, q, v, V);
			/*if (u == 1.0 && v == 1.0) {
				cout << i << " " << j<<" ";
				cout << N1<<" ";
				cout << N2<<endl;
			}*/
			point.x += N1 * N2 * controlPoints.at(i * z + j).x;
			point.y += N1 * N2 * controlPoints.at(i * z + j).y;
			point.z += N1 * N2 * controlPoints.at(i * z + j).z;
		}
	}

	return point;
}

void cursorPosCallback(GLFWwindow* window, double xPos, double yPos)
{

}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (showCPoints) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			double	x, y;

			glfwGetCursorPos(window, &x, &y);
			dragged = getActivePoint(controlPoints, 0.05f, x, window_height - y);
		}
	}	
	//cout << dragged << "\n";

}

/** Kiszámoljuk a kamera mátrixokat. */
void computeCameraMatrix() {
	/* A paraméterek rendre: az új koordinátarendszerünk középpontja (hol a kamera), merre néz a kamera, mit tekintünk ,,fölfele" iránynak */
	//view = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUpVector);
	view = glm::lookAt(cameraPos, cameraTarget, cameraUpVector);
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void generateControllPoints(int paramX, int paramZ) {

	//dll-hez szükséges utasítások
	//controlPoints.clear();

	//x = paramX;
	//z = paramZ;

	do {
		cout << "Pontok X mentén (min 3 max 10): ";
		cin >> x;

		cout << "Pontok Z mentén (min 3 max 10): ";
		cin >> z;
	} while (x <= 2 || z <= 2 || x > 10 || z > 10);
	

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

void generateAxesToDraw() {
	pointsToDraw.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
	pointsToDraw.push_back(glm::vec3(-1.0f, 0.0f, 0.0f));
	pointsToDraw.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
	pointsToDraw.push_back(glm::vec3(0.0f, -1.0f, 0.0f));
	pointsToDraw.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
	pointsToDraw.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
}

std::vector<GLfloat> generateKnots(int points) {
	std::vector<GLfloat> U;
	GLfloat u = 0.0f, increment = (1.0f / (points - 3));
	if (points >= 4) {
		U.push_back(0.0f);
		U.push_back(0.0f);
		U.push_back(0.0f);
		while (u < 1.0f) {
			U.push_back(u);
			u += increment;
		}
		U.push_back(1.0f);
		U.push_back(1.0f);
		U.push_back(1.0f);
		U.push_back(1.0f);
	}
	else {
		U.push_back(0.0f);
		U.push_back(0.0f);
		while (u < 1.0f) {
			U.push_back(u);
			u += increment;
		}
		U.push_back(1.0f);
		U.push_back(1.0f);
		U.push_back(1.0f);
	}
	

	return U;
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

	//Bezier felület
	if (surface_id == 1) {
		GLfloat u = 0.0f, v = 0.0f, increment = 1.0f / lineCount, uIcrement = (1.0f / density) / (x - 1), vIcrement = (1.0f / density) / (z - 1);

		while (u < 1.0f) {
			v = 0.0f;
			while (v < 1.0f) {
				pointsToDraw.push_back(BezierPoints(u, v));
				//cout << glm::to_string(BezierPoints(u, v)) << "\n";
				v += increment;				
			}
			v = 1.0f;
			pointsToDraw.push_back(BezierPoints(u, v));
			//cout << glm::to_string(BezierPoints(u, v)) << "\n";
			u += uIcrement;
		}
		u = 1.0f, v = 0.0f;
		while (v < 1.0f) {
			pointsToDraw.push_back(BezierPoints(u, v));
			//cout << glm::to_string(BezierPoints(u, v)) << "\n";
			v += increment;
		}
		v = 1.0f;
		pointsToDraw.push_back(BezierPoints(u, v));

		v = 0.0f;
		while (v < 1.0f) {
			u = 0.0f;
			while (u < 1.0f) {
				pointsToDraw.push_back(BezierPoints(u, v));
				//cout << glm::to_string(BezierPoints(u, v)) << "\n";
				u += increment;
			}
			u = 1.0f;
			pointsToDraw.push_back(BezierPoints(u, v));
			//cout << glm::to_string(BezierPoints(u, v)) << "\n";
			v += vIcrement;
		}
		v = 1.0f, u = 0.0f;
		while (u < 1.0f) {
			pointsToDraw.push_back(BezierPoints(u, v));
			//cout << glm::to_string(BezierPoints(u, v)) << "\n";
			u += increment;
		}
		u = 1.0f;
		pointsToDraw.push_back(BezierPoints(u, v));
		//cout << "------------------------\n";
	}


	//B-Spline felület
	else if (surface_id == 2) {

		std::vector<GLfloat> U;
		std::vector<GLfloat> V;

		U = generateKnots(x);
		V = generateKnots(z);

		//cout << "U: ";
		for (int i = 0; i < U.size(); i++) {
			//cout << U[i] << ", ";
		}
		//cout << "\n";
		//cout << "V: ";
		for (int i = 0; i < V.size(); i++) {
			//cout << V[i] << ", ";
		}
		//cout << "\n";

		GLfloat u = 0.0f, v = 0.0f, increment = 1.0f / lineCount, uIcrement = (1.0f / density) / (x - 1), vIcrement = (1.0f / density) / (z - 1);

		while (u < 1.0f) {
			v = 0.0f;
			while (v < 1.0f) {
				pointsToDraw.push_back(BSplinePoints(u, v, U, V));
				//cout << glm::to_string(BSplinePoints(u, v, U, V)) << "\n";
				v += increment;
			}
			v = 0.999999f;
			pointsToDraw.push_back(BSplinePoints(u, v, U, V));
			//cout << glm::to_string(BSplinePoints(u, v, U, V)) << "\n";
			u += uIcrement;
		}
		u = 0.99999f, v = 0.0f;
		while (v < 1.0f) {
			pointsToDraw.push_back(BSplinePoints(u, v, U, V));
			//cout << glm::to_string(BSplinePoints(u, v, U, V)) << "\n";
			v += increment;
		}
		v = 0.99999f;
		pointsToDraw.push_back(BSplinePoints(u, v, U, V));

		v = 0.0f;
		while (v < 1.0f) {
			u = 0.0f;
			while (u < 1.0f) {
				pointsToDraw.push_back(BSplinePoints(u, v, U, V));
				//cout << glm::to_string(BSplinePoints(u, v, U, V)) << "\n";
				u += increment;
			}
			u = 0.99999f;
			pointsToDraw.push_back(BSplinePoints(u, v, U, V));
			//cout << glm::to_string(BSplinePoints(u, v, U, V)) << "\n";
			v += vIcrement;
		}
		v = 0.99999f, u = 0.0f;
		while (u < 1.0f) {
			pointsToDraw.push_back(BSplinePoints(u, v, U, V));
			//cout << glm::to_string(BSplinePoints(u, v, U, V)) << "\n";
			u += increment;
		}
		u = 0.99999f;
		pointsToDraw.push_back(BSplinePoints(u, v, U, V));


	}

	generateAxesToDraw();
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

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if ((action == GLFW_PRESS) && (key == GLFW_KEY_ESCAPE))
		cleanUpScene();

	if (action == GLFW_PRESS)
		keyboard[key] = GL_TRUE;
	else if (action == GLFW_RELEASE)
		keyboard[key] = GL_FALSE;

	if (key == GLFW_KEY_G && action == GLFW_PRESS) {
		showGrid = !showGrid;
	}

	if (key == GLFW_KEY_C && action == GLFW_PRESS) {
		showCPoints = !showCPoints;
		if (!showCPoints) {
			dragged = -1;
		}
	}

	if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		surface_id = 1;
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		surface_id = 2;
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		for (int i = 0; i < controlPoints.size(); i++) {
			controlPoints[i].y = 0.0f;
		}
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void init(GLFWwindow* window) {
	renderingProgram = createShaderProgram();
	generateControllPoints(3, 3);
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
	numberOfPointsToDraw = glGetUniformLocation(renderingProgram, "numberOfPointsToDraw");
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

	if (controllPointsToDefault) {
		for (int i = 0; i < controlPoints.size(); i++) {
			controlPoints[i].y = 0.0f;
		}
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		dragged = -1;
		controllPointsToDefault = false;
	}

	if (numberOfControllPointsChanged) {
		generateControllPoints(temp_X, temp_Z);
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		numberOfControllPointsChanged = false;
		dragged = -1;
	}

	if (surfaceIdChange) {
		surface_id = temp_surface_id;
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		surfaceIdChange = false;
	}

	if (densityChange) {
		density = temp_density;
		generatePointsToDraw();
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, pointsToDraw.size() * sizeof(glm::vec3), pointsToDraw.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		densityChange = false;
	}
		

	//translateM = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	//model = translateM;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	computeCameraMatrix();

	glProgramUniform1f(renderingProgram, pointNum, controlPoints.size());
	glProgramUniform1f(renderingProgram, selectedPoint, dragged);
	glProgramUniform1f(renderingProgram, numberOfPointsToDraw, pointsToDraw.size());

	/*Csatoljuk a vertex array objektumunkat. */
	glBindVertexArray(VAO[0]);
	size_t begin = 0;

	//Bezier felület kirajzolása
		glProgramUniform1f(renderingProgram, drawingPoints, 3.0f);
		begin = controlPoints.size() * 2;
		for (size_t i = 0; i <= density * (x - 1); i++) {
			glDrawArrays(GL_LINE_STRIP, begin, lineCount + 1);
			begin += lineCount + 1 ;
		}

		for (size_t i = 0; i <= density * (z - 1); i++) {
			glDrawArrays(GL_LINE_STRIP, begin, lineCount + 1);
			begin += lineCount + 1;
		}
	

	if (showCPoints) {
		glProgramUniform1f(renderingProgram, drawingPoints, 1.0f);
		glPointSize(10);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawArrays(GL_POINTS, 0, controlPoints.size());
	}
	
	glProgramUniform1f(renderingProgram, drawingPoints, 2.0f);

	//Kontrollpontháló kirajzolása
	if (showGrid) {
		begin = 0;
		for (size_t i = 0; i < x; i++)
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
	}
	
	glProgramUniform1f(renderingProgram, drawingPoints, 4.0f);
	//tengely kirajzolása
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//glDrawArrays(GL_POINTS, (pointsToDraw.size() - 6), pointsToDraw.size());
	glDrawArrays(GL_LINE_STRIP, pointsToDraw.size() - 6, 2);
	glDrawArrays(GL_LINE_STRIP, pointsToDraw.size() - 4, 2);
	glDrawArrays(GL_LINE_STRIP, pointsToDraw.size() - 2, 2);

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

DWORD dwThreadId = NULL;

extern "C"
{
	__declspec(dllexport) void Init()
	{
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, &dwThreadId);
	}

	__declspec(dllexport) void HandleControllPointsShow(bool value)
	{
		showCPoints = value;
	}

	__declspec(dllexport) void HandleGridShow(bool value)
	{
		showGrid = value;
	}

	__declspec(dllexport) void ControllPointsToDefault()
	{
		controllPointsToDefault = true;
	}

	__declspec(dllexport) void HandleNumberOfControllPointsChange(int param_X, int param_Z)
	{
		temp_X = param_X;
		temp_Z = param_Z;
		numberOfControllPointsChanged = true;
	}

	__declspec(dllexport) void HandleSurfaceIdChange(int param_surface_id)
	{
		temp_surface_id = param_surface_id;
		surfaceIdChange = true;
	}

	__declspec(dllexport) void HandleDensityChange(int param_density)
	{
		temp_density = param_density;
		densityChange = true;
	}
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		if (dwThreadId != NULL) {
			TerminateThread((HANDLE)dwThreadId, 0);
		}
		break;
	default:
		break;
	}
	return TRUE;
}
