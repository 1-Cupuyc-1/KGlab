#include "Render.h"
#include <Windows.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "Texture.h"


#include "ObjLoader.h"


#include "debout.h"
#include <random> 



//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = false;

bool show_grid = true;



//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL* sender, KeyEventArg arg)
{
	//конвертируем код клавиши в букву
	auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

	switch (key)
	{
	case 'L':
		lightning = !lightning;
		break;
	case 'T':
		texturing = !texturing;
		break;
	case 'A':
		alpha = !alpha;
		break;
	case VK_TAB:
		show_grid = !show_grid;
		break;
	}
}

//умножение матриц c[M1][N1] = a[M1][N1] * b[M2][N2]
template<typename T, int M1, int N1, int M2, int N2>
void MatrixMultiply(const T* a, const T* b, T* c)
{
	for (int i = 0; i < M1; ++i)
	{
		for (int j = 0; j < N2; ++j)
		{
			c[i * N2 + j] = T(0);
			for (int k = 0; k < N1; ++k)
			{
				c[i * N2 + j] += a[i * N1 + k] * b[k * N2 + j];
			}
		}
	}
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером

ObjModel f;


Shader cassini_sh;
Shader phong_sh;
Shader vb_sh;
Shader simple_texture_sh;

Texture stankin_tex, vb_tex, monkey_tex;

const int MAX_SNAKE_LENGTH = 100; 
int snake_length = 3;
int score = 0;
float snake_radius = 0.1f;   
float segment_distance = 0.2f;
const double cube_size = 10;
struct SnakeSegment {
	float position[3]; 
	float color[3];  
};

enum Direction {
	DIR_RIGHT,
	DIR_LEFT,
	DIR_UP,
	DIR_DOWN,
	DIR_FORWARD,
	DIR_BACKWARD
};

SnakeSegment snake[MAX_SNAKE_LENGTH];
float snake_speed = 0.6f;      
Direction current_direction = DIR_FORWARD, next_direction = DIR_FORWARD;

struct Apple {
	float position[3];
	float color[3] = { 1.0f, 0.0f, 0.0f };
};

Apple apple;
void spawnApple() {
	float range = cube_size / 2.0f - 1.0f;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(-range, range);

	apple.position[0] = std::round(dist(gen));
	apple.position[1] = std::round(dist(gen));
	apple.position[2] = std::round(dist(gen));
}

void drawApple() {
	GLUquadric* quad = gluNewQuadric();
	glPushMatrix();
	glTranslatef(apple.position[0], apple.position[1], apple.position[2]);
	glColor3fv(apple.color);
	gluSphere(quad, snake_radius * 1.2f, 16, 16);
	glPopMatrix();
	gluDeleteQuadric(quad);
}

void initSnake() {
	snake_length = 3;
	score = 0;
	vb_sh.VshaderFileName = "shaders/v.vert";
	vb_sh.FshaderFileName = "shaders/vb.frag";
	vb_sh.LoadShaderFromFile();
	vb_sh.Compile();

	simple_texture_sh.VshaderFileName = "shaders/v.vert";
	simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
	simple_texture_sh.LoadShaderFromFile();
	simple_texture_sh.Compile();

	stankin_tex.LoadTexture("textures/stankin.png");
	vb_tex.LoadTexture("textures/vb.png");
	monkey_tex.LoadTexture("textures/monkey.png");


	f.LoadModel("models//monkey.obj_m");

	for (int i = 0; i < MAX_SNAKE_LENGTH; i++) {
		snake[i].position[0] = 0.0f;
		snake[i].position[1] = 0.0f;
		snake[i].position[2] = i * segment_distance;

		if (i == 0) {
			snake[i].color[0] = 1.0f;
			snake[i].color[1] = 0.0f;
			snake[i].color[2] = 0.0f;
		}
		else if (i == MAX_SNAKE_LENGTH - 1) {
			snake[i].color[0] = 0.0f;
			snake[i].color[1] = 0.3f;
			snake[i].color[2] = 1.0f;
		}
		else {
			float ratio = static_cast<float>(i) / MAX_SNAKE_LENGTH;
			snake[i].color[0] = 0.2f * ratio;
			snake[i].color[1] = 0.7f - 0.5f * ratio;
			snake[i].color[2] = 0.5f + 0.5f * ratio;
		}
	}
}
void updateSnake(double delta_time) {
	float step = snake_speed * delta_time;
	float* head = snake[0].position;
	const float epsilon = 0.01f;
	bool is_on_cell =
		std::fabs(head[0] - std::round(head[0])) < epsilon &&
		std::fabs(head[1] - std::round(head[1])) < epsilon &&
		std::fabs(head[2] - std::round(head[2])) < epsilon;

	if (is_on_cell) {
		current_direction = next_direction;
	}
	switch (current_direction) {
	case DIR_RIGHT:   head[0] += step; break;
	case DIR_LEFT:    head[0] -= step; break;
	case DIR_UP:      head[1] += step; break;
	case DIR_DOWN:    head[1] -= step; break;
	case DIR_FORWARD: head[2] -= step; break;
	case DIR_BACKWARD:head[2] += step; break;
	}
	GLUquadric* quad = gluNewQuadric();
	float dx = head[0] - apple.position[0];
	float dy = head[1] - apple.position[1];
	float dz = head[2] - apple.position[2];
	float distance = sqrtf(dx * dx + dy * dy + dz * dz);

	if (distance < snake_radius * 2) {
		score++;
		spawnApple();
		if (snake_length < MAX_SNAKE_LENGTH) {
			snake[snake_length] = snake[snake_length - 1];
			snake_length++;
		}
	}


	for (int i = snake_length - 1; i > 0; i--) {
		float* pos = snake[i].position;
		float diff[4];

		if (i == 0) {
			diff[0] = 1.0f; diff[1] = 0.0f; diff[2] = 0.0f; diff[3] = 1.0f;
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
			glPushMatrix();
			glTranslatef(pos[0], pos[1], pos[2]);
			gluSphere(quad, snake_radius * 1.5f, 20, 20);
			glPopMatrix();
			continue;
		}
		else if (i == snake_length - 1) {
			diff[0] = 0.0f; diff[1] = 0.3f; diff[2] = 1.0f; diff[3] = 1.0f;
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
			glPushMatrix();
			glTranslatef(pos[0], pos[1], pos[2]);
			gluSphere(quad, snake_radius * 0.8f, 16, 16);
			glPopMatrix();
			continue;
		}
		diff[0] = snake[i].color[0];
		diff[1] = snake[i].color[1];
		diff[2] = snake[i].color[2];
		diff[3] = 1.0f;
		glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
		glPushMatrix();
		glTranslatef(pos[0], pos[1], pos[2]);
		gluSphere(quad, snake_radius, 16, 16);
		glPopMatrix();
	}
	for (int i = 0; i < 3; i++) {
		if (head[i] < -cube_size / 2 || head[i] > cube_size / 2) {
			initSnake();
			spawnApple();
			return;     
		}
	}
	for (int i = 1; i < snake_length; i++) {
		float* segment = snake[i].position;
		float dx = head[0] - segment[0];
		float dy = head[1] - segment[1];
		float dz = head[2] - segment[2];
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		if (dist < snake_radius * 1.1f) {
			initSnake();
			spawnApple();
			return;
		}
	}
	for (int i = snake_length - 1; i > 0; i--) {
		float* prev = snake[i - 1].position;
		float* curr = snake[i].position;
		float dx = prev[0] - curr[0];
		float dy = prev[1] - curr[1];
		float dz = prev[2] - curr[2];
		float len = sqrtf(dx * dx + dy * dy + dz * dz);
		if (len > 0.001f) {
			dx /= len; dy /= len; dz /= len;
			float diff = len - segment_distance;
			curr[0] += dx * diff;
			curr[1] += dy * diff;
			curr[2] += dz * diff;
		}
	}
}

void drawSnake() {
	GLUquadric* quad = gluNewQuadric();
	gluQuadricNormals(quad, GLU_SMOOTH);
	for (int i = 0; i < snake_length; i++){
		float* pos = snake[i].position;
		float diff[] = { snake[i].color[0], snake[i].color[1], snake[i].color[2], 1.0f };
		glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
		glPushMatrix();
		glTranslatef(pos[0], pos[1], pos[2]);
		gluSphere(quad, snake_radius, 16, 16);
		glPopMatrix();
	}
	gluDeleteQuadric(quad);
}
void DrawCube() {
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glColor3d(0.6, 0.6, 0.6);
	glLineWidth(1);
	double h = cube_size / 2.0;
	int half = static_cast<int>(h);
	for (int xi = -half; xi <= half; xi++) {
		for (int yi = -half; yi <= half; yi++) {
			glBegin(GL_LINES);
			glVertex3d(xi, yi, -h);
			glVertex3d(xi, yi, h);
			glEnd();
		}
		for (int zi = -half; zi <= half; zi++) {
			glBegin(GL_LINES);
			glVertex3d(xi, -h, zi);
			glVertex3d(xi, h, zi);
			glEnd();
		}
	}
	for (int yi = -half; yi <= half; yi++) {
		for (int zi = -half; zi <= half; zi++) {
			glBegin(GL_LINES);
			glVertex3d(-h, yi, zi);
			glVertex3d(h, yi, zi);
			glEnd();
		}
	}
	if (lightning) glEnable(GL_LIGHTING);
}

void keyControlSnake(OpenGL* sender, KeyEventArg arg) {
	switch (arg.key) {
	case VK_LEFT:  next_direction = DIR_LEFT; break;
	case VK_RIGHT: next_direction = DIR_RIGHT; break;
	case VK_UP:    next_direction = DIR_UP; break;
	case VK_DOWN:  next_direction = DIR_DOWN; break;
	case 'S': case 's': next_direction = DIR_FORWARD; break;
	case 'W': case 'w': next_direction = DIR_BACKWARD; break;
	}
}

void initRender()
{
	initSnake();
	spawnApple();

	//==============НАСТРОЙКА ТЕКСТУР================
	//4 байта на хранение пикселя
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);



	//================НАСТРОЙКА КАМЕРЫ======================
	camera.caclulateCameraPos();

	//привязываем камеру к событиям "движка"
	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
	//==============НАСТРОЙКА СВЕТА===========================
	//привязываем свет к событиям "движка"
	gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
	gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
	gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
	//========================================================
	//====================Прочее==============================
	gl.KeyDownEvent.reaction(switchModes);
	text.setSize(512, 220);
	gl.KeyDownEvent.reaction(keyControlSnake);
	//========================================================

	camera.setPosition(0, -15, 15);
	light.SetPosition(0, -15, 0);

}
float view_matrix[16];
double full_time = 0;
int location = 0;
void Render(double delta_time)
{


	full_time += delta_time;

	//натройка камеры и света
	//в этих функциях находятся OGLные функции
	//которые устанавливают параметры источника света
	//и моделвью матрицу, связанные с камерой.

	if (gl.isKeyPressed('F')) //если нажата F - свет из камеры
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	//забираем моделвью матрицу сразу после установки камера
	//так как в ней отсуствуют трансформации glRotate...
	//она, фактически, является видовой.
	glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);



	light.SetUpLight();

	//рисуем оси
	gl.DrawAxes();



	glBindTexture(GL_TEXTURE_2D, 0);

	//включаем нормализацию нормалей
	//чтобв glScaled не влияли на них.

	glEnable(GL_NORMALIZE);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	//включаем режимы, в зависимости от нажания клавиш. см void switchModes(OpenGL *sender, KeyEventArg arg)
	if (lightning)
		glEnable(GL_LIGHTING);
	if (texturing)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0); //сбрасываем текущую текстуру
	}

	if (alpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	//=============НАСТРОЙКА МАТЕРИАЛА==============


	//настройка материала, все что рисуется ниже будет иметь этот метериал.
	//массивы с настройками материала
	float  amb[] = { 0.2, 0.2, 0.1, 1. };
	float dif[] = { 0.4, 0.65, 0.5, 1. };
	float spec[] = { 0.9, 0.8, 0.3, 1. };
	float sh = 0.2f * 256;

	//фоновая
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//дифузная
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//зеркальная
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	//размер блика
	glMaterialf(GL_FRONT, GL_SHININESS, sh);

	//чтоб было красиво, без квадратиков (сглаживание освещения)
	glShadeModel(GL_SMOOTH); //закраска по Гуро      
	//(GL_SMOOTH - плоская закраска)

//============ РИСОВАТЬ ТУТ ==============
	updateSnake(delta_time);
	drawSnake();
	if (show_grid)
		DrawCube();
	drawApple();

	//===============================================


	//сбрасываем все трансформации
	glLoadIdentity();
	camera.SetUpCamera();
	Shader::DontUseShaders();
	//рисуем источник света
	light.DrawLightGizmo();

	//================Сообщение в верхнем левом углу=======================
	glActiveTexture(GL_TEXTURE0);
	//переключаемся на матрицу проекции
	glMatrixMode(GL_PROJECTION);
	//сохраняем текущую матрицу проекции с перспективным преобразованием
	glPushMatrix();
	//загружаем единичную матрицу в матрицу проекции
	glLoadIdentity();

	//устанавливаем матрицу паралельной проекции
	glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

	//переключаемся на моделвью матрицу
	glMatrixMode(GL_MODELVIEW);
	//сохраняем матрицу
	glPushMatrix();
	//сбразываем все трансформации и настройки камеры загрузкой единичной матрицы
	glLoadIdentity();

	//отрисованное тут будет визуалзироватся в 2д системе координат
	//нижний левый угол окна - точка (0,0)
	//верхний правый угол (ширина_окна - 1, высота_окна - 1)


	std::wstringstream ss;
	ss << std::fixed << std::setprecision(3);
	ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
	ss << "TAB - " << (show_grid ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"сетка" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
	ss << L"Коорд. света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5) << delta_time << std::endl;
	ss << L"full_time: " << std::setprecision(2) << full_time << std::endl;
	ss << L"Очки: " << score << std::endl;



	text.setPosition(10, gl.getHeight() - 10 - 220);
	text.setText(ss.str().c_str());

	text.Draw();

	//восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();


}