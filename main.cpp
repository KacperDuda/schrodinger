#include "pch.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <complex>
#include <cstdlib>

typedef sf::Event sfe;
typedef sf::Keyboard sfk;

////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////

// importing and processing 2D .sim file

float framerate = 30;
bool colored = true;
int X_n, Y_n;
float multiplier = 10.;
std::vector<std::vector<std::vector<std::vector<std::complex<double>>>>> frames;

struct Config {
    int layers;
    int dimensions;
    std::vector<int> size;
    double dr;
    double dt;
    double snapshotPerTime;
};

Config config;

Config parseConfig(const std::string& configString) {
    Config config;
    std::unordered_map<std::string, std::variant<int, double, std::vector<int>>> configMap;
    
    std::stringstream ss(configString);
    std::string param;

    while (std::getline(ss, param, ';')) {
        std::stringstream paramStream(param);
        std::string key;
        std::getline(paramStream, key, ',');

        std::string value;
        std::vector<std::string> values;
        while (std::getline(paramStream, value, ',')) {
            values.push_back(value);
        }
        
        if (key == "size") {
            std::vector<int> intValues;
            for (const auto& val : values) {
                intValues.push_back(std::stoi(val));
            }
            config.size = intValues;
        } else {
            double doubleValue = values.size() == 1 ? std::stod(values[0]) : std::stod(values[0]);
            if (key == "dr") config.dr = doubleValue;
            else if (key == "dt") config.dt = doubleValue;
            else if (key == "snapshotPerTime") config.snapshotPerTime = doubleValue;
        }
        
        if (key == "layers") config.layers = std::stoi(values[0]);
        if (key == "dimentions") config.dimensions = std::stoi(values[0]);
    }

    return config;
}

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> elements;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        elements.push_back(item);
    }
    return elements;
}

std::vector<std::vector<std::vector<std::complex<double>>>> parseLine(const std::string& line, size_t layers, const std::vector<int>& size) {
    auto numbersStr = splitString(line, ';');

    std::vector<std::vector<std::vector<std::complex<double>>>> result(layers,
        std::vector<std::vector<std::complex<double>>>(size[0], 
        std::vector<std::complex<double>>(size[1])));


    // int index = 0;
    // for (int l = 0; l < layers; ++l) {
    //     for (int i = 0; i < size[0]; ++i) {
    //         for (int j = 0; j < size[1]; ++j) {
    //             auto parts = splitString(numbersStr[index++], '\'');
    //             result[l][i][j] = std::complex<double>(std::stod(parts[0]), std::stod(parts[1]));
    //         }
    //     }
    // }

    int index = 0; // ta pętla generuje problemy
    for (int l = 0; l < layers; ++l) {
        for (int i = 0; i < size[0]; ++i) {
            for (int j = 0; j < size[1]; ++j) {
                
                auto parts = splitString(numbersStr[index++], '\'');


                try {
                result[l][i][j] = std::complex<double>(
                    std::stod(parts[0]),
                    std::stod(parts[1])
                );
                } catch (const std::invalid_argument& e) {
                    // std::cerr << "Error: Invalid argument at index " << index - 1
                    //           << ". Real part: " << parts[0]
                    //           << ", Imaginary part: " << parts[1] << " " << i << " " << j << std::endl;
                } catch (const std::out_of_range& e) { // 1e-323 is too small for double :(
                    // std::cerr << "Error: Out of range at index " << index - 1
                    //           << ". Real part: " << parts[0]
                    //           << ", Imaginary part: " << parts[1] << " " << i << " " << j << std::endl;
                }
            }
        }
    }
        // std::abort();


    return result;
}

void processFile(std::string filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return;
    }


    std::string title;
    std::getline(file, title);

    std::string configString;
    std::getline(file, configString);


    config = parseConfig(configString);


    int iterator = 0;
    std::string frameLine;
    while (std::getline(file, frameLine)) {
        iterator++;
        if (iterator % 1000 == 0)
            std::cout << iterator/1000 << std::endl;
        frames.push_back(parseLine(frameLine, config.layers, config.size));
    }

    // frames[line][layer][x][y]
    // std::cout << std::norm(frames[50][0][30][20]) << std::endl;
    X_n = frames[0][0].size();
    Y_n = frames[0][0][0].size();


    file.close();
}




/////////////////////////////////////////////

struct Spherical
{
    float distance, theta, phi;
    Spherical(float gdistance, float gtheta, float gphi) : distance(gdistance), theta(gtheta), phi(gphi) { }
    float getX() { return distance * cos(phi) * cos(theta); }
    float getY() { return distance * sin(phi); }
    float getZ() { return distance * cos(phi) * sin(theta); }
};

Spherical camera(3.0f, 1.0f, 0.2f), light_position(4.0f, 0.2f, 1.2f);
sf::Vector3f pos(0.0f, 0.0f, 0.0f), scale(1.0f, 1.0f, 1.0f), rot(0.0f, 0.0f, 0.0f);
bool perspective_projection = true;
float fov = 45.0f;
float timer = 0.0;
bool timerrunning = true;

float *pos_offsets[3] = { &pos.x, &pos.y, &pos.z };
float* scale_offsets[3] = { &scale.x, &scale.y, &scale.z };
float* rot_offsets[3] = { &rot.x, &rot.y, &rot.z };

void initOpenGL(void)
{
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat light_ambient_global[4] = { 0.5,0.5,0.5,1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, light_ambient_global);
}

void reshapeScreen(sf::Vector2u size)
{
    glViewport(0, 0, (GLsizei)size.x, (GLsizei)size.y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (perspective_projection) gluPerspective(fov, (GLdouble)size.x / (GLdouble)size.y, 0.1, 100.0);
    else glOrtho(-1.245 * ((GLdouble)size.x / (GLdouble)size.y), 1.245 * ((GLdouble)size.x / (GLdouble)size.y), -1.245, 1.245, -3.0, 12.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

sf::Vector3f cross_product(sf::Vector3f u, sf::Vector3f v)
{
    sf::Vector3f res;
    res.x = u.y * v.z - u.z * v.y;
    res.y = u.z * v.x - u.x * v.z;
    res.z = u.x * v.y - u.y * v.x;
    return res;
}

void glVertexsf(sf::Vector3f v)
{
    glVertex3f(v.x, v.y, v.z);
}

void glNormalsf(sf::Vector3f v)
{
    glNormal3f(v.x, v.y, v.z);
}



double getValue(int x, int y, double timer) {
    double maxtime = frames.size()/framerate;
    int frameindex = (int) (timer*frames.size()/maxtime);
    // std::cout << frameindex << " " << maxtime << std::endl;
    if(frameindex >= frames.size()) // out or rnage!
        return 0;
    return multiplier * std::norm(frames[frameindex][0][x][y]);
}

double getAngle(int x, int y, double tinmer) {
        double maxtime = frames.size()/framerate;
    int frameindex = (int) (timer*frames.size()/maxtime);
    // std::cout << frameindex << " " << maxtime << std::endl;
    if(frameindex >= frames.size()) // out or rnage!
        return 0;
    return std::arg(frames[frameindex][0][x][y]);
}

void drawScene()
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    Spherical north_of_camera(camera.distance, camera.theta, camera.phi + 0.01f);
    gluLookAt(camera.getX(), camera.getY(), camera.getZ(),
        0.0, 0.0, 0.0,
        north_of_camera.getX(), north_of_camera.getY(), north_of_camera.getZ());
    GLfloat light0_position[4] = { light_position.getX(), light_position.getY(), light_position.getZ(), 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);

    //uklad
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
        glColor3f(1.0, 0.0, 0.0); glVertex3f(0, 0, 0); glVertex3f(1.0, 0, 0);
        glColor3f(0.0, 1.0, 0.0); glVertex3f(0, 0, 0); glVertex3f(0, 1.0, 0);
        glColor3f(0.0, 0.0, 1.0); glVertex3f(0, 0, 0); glVertex3f(0, 0, 1.0);
    glEnd();

    //Linie przerywane
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(2, 0xAAAA);
    glBegin(GL_LINES);
        glColor3f(1.0, 0.0, 0.0); glVertex3f(0, 0, 0); glVertex3f(-1.0, 0, 0);
        glColor3f(0.0, 1.0, 0.0); glVertex3f(0, 0, 0); glVertex3f(0, -1.0, 0);
        glColor3f(0.0, 0.0, 1.0); glVertex3f(0, 0, 0); glVertex3f(0, 0, -1.0);
    glEnd();
    glDisable(GL_LINE_STIPPLE);

    //transformacje
    glTranslatef(pos.x, pos.y, pos.z);
    glRotatef(rot.x, 1, 0, 0);
    glRotatef(rot.y, 0, 1, 0);
    glRotatef(rot.z, 0, 0, 1);
    glScalef(scale.x, scale.y, scale.z);

    glEnable(GL_LIGHTING);

    sf::Vector3f u, v, res[4], flag[X_n][Y_n], flag_n[X_n][Y_n];

    for (int x = 0; x < X_n; x++)
        for (int y = 0; y < Y_n; y++)
        {
            flag[x][y].x = (x - X_n/2) * 0.02f;
            flag[x][y].y =  multiplier * getValue(x, y, timer);
            flag[x][y].z = (y - Y_n/2) * 0.02f;
        }
    for (int x = 1; x < X_n - 1; x++)
        for (int y = 1; y < Y_n - 1; y++)
        {
            u = flag[x][y + 1] - flag[x][y];
            v = flag[x + 1][y] - flag[x][y];
            res[0] = cross_product(u, v);

            u = flag[x + 1][y] - flag[x][y];
            v = flag[x][y - 1] - flag[x][y];
            res[1] = cross_product(u, v);

            u = flag[x][y - 1] - flag[x][y];
            v = flag[x - 1][y] - flag[x][y];
            res[2] = cross_product(u, v);

            u = flag[x - 1][y] - flag[x][y];
            v = flag[x][y + 1] - flag[x][y];
            res[3] = cross_product(u, v);

            flag_n[x][y] = (res[0] + res[1] + res[2] + res[3]);
        }


    glColor3f(0.9765f, 0.85f, 0.3671f);
    glBegin(GL_QUADS);
    // glNormal3f(0, 1, 0);
    for (int x = 1; x < X_n-2; x++)
        for (int y = 1; y < Y_n-2; y++)
        {
            if(colored) { // do wyrzucenia przed pętle
                double angle = getAngle(x, y, timer);

                float hue = ((angle + M_PI) / (2 * M_PI)) * 360.0f;
                float c = 1.0f;
                float xx = c * (1.0f - fabs(fmod(hue / 60.0f, 2) - 1.0f));
                float m = 0.0f;
                float r, g, b;

                if (hue < 60)       { r = c; g = xx; b = 0; }
                else if (hue < 120) { r = xx; g = c; b = 0; }
                else if (hue < 180) { r = 0; g = c; b = xx; }
                else if (hue < 240) { r = 0; g = xx; b = c; }
                else if (hue < 300) { r = xx; g = 0; b = c; }
                else                { r = c; g = 0; b = xx; }
                
                glColor3f(r + m, g + m, b + m);
            }

            glNormalsf(flag_n[x][y]);         glVertexsf(flag[x][y]);
            glNormalsf(flag_n[x + 1][y]);     glVertexsf(flag[x + 1][y]);
            glNormalsf(flag_n[x + 1][y + 1]); glVertexsf(flag[x + 1][y + 1]);
            glNormalsf(flag_n[x][y + 1]);     glVertexsf(flag[x][y + 1]);
        }
    glEnd();

    glDisable(GL_LIGHTING);
    glColor3f(0, 0, 0.8f);
    glBegin(GL_LINES);
    for (int x = 1; x < X_n - 1; x++)
        for (int y = 1; y < Y_n-1; y++)
        {
            glVertexsf(flag[x][y]);
            flag[x][y] += flag_n[x][y];
            glVertexsf(flag[x][y]);
        }
    glEnd();
    glEnable(GL_LIGHTING);
}

void updateTime(sf::Clock& deltaClock) {
    timer += deltaClock.getElapsedTime().asSeconds();
    if(timer >= frames.size()/framerate)
        timer = 0;
}

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);
    if(args.size() < 2) {
        std::cerr << "No file specified." << std::endl;
        return 1;
    }
    std::string file_name = args[1];

    std::cout << "Processing file..." << std::endl;
    std::string basePath = "/home/xacper/Desktop/schrodinger_numerical/simulations/";
    processFile(basePath + file_name);
    std::cout << "Finished." << std::endl;

    bool running = true;
    sf::ContextSettings context(24, 0, 0, 4, 5);
    sf::RenderWindow window(sf::VideoMode(1280, 1024), "Symulacje Kwantowe", 7U, context);
    sf::Clock deltaClock;

    bool is_ok = ImGui::SFML::Init(window);
    std::cout << is_ok << std::endl;

    window.setVerticalSyncEnabled(true);

    reshapeScreen(window.getSize());
    initOpenGL();

    while (running)
    {
        sfe event;
        while (window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sfe::Closed || (event.type == sfe::KeyPressed && event.key.code == sfk::Escape)) running = false;
            if (event.type == sfe::Resized) reshapeScreen(window.getSize());
        }

        if (sfk::isKeyPressed(sfk::Left))  { camera.theta -= 0.0174533f; if (camera.theta < 0.0f) camera.theta = (float)(2.0*std::numbers::pi); }
        if (sfk::isKeyPressed(sfk::Right)) { camera.theta += 0.0174533f; if (camera.theta > (float)(2.0*std::numbers::pi)) camera.theta = (float)(0); }
        if (sfk::isKeyPressed(sfk::Up))    { camera.phi += 0.0174533f;  if (camera.phi > (float)std::numbers::pi) camera.phi = 0.0f; }
        if (sfk::isKeyPressed(sfk::Down))  { camera.phi -= 0.0174533f; if (camera.phi < 0.0f) camera.phi = (float)std::numbers::pi; }

        drawScene();
        if(timerrunning)
            updateTime(deltaClock);
        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::Begin("Camera");
            ImGui::SliderFloat("R", &camera.distance, 0.5f, 10.0f);
            ImGui::SliderAngle("theta", &camera.theta, 0.0f, 360.0f);
            ImGui::SliderAngle("phi", &camera.phi, 0.0f, 180.0f);
            if (ImGui::Checkbox("Perspective projection", &perspective_projection)) reshapeScreen(window.getSize());
            if (ImGui::SliderFloat("FoV", &fov, 10.0f, 90.0f)) reshapeScreen(window.getSize());
        ImGui::End();

        ImGui::Begin("Transformations");
         ImGui::SliderFloat3("Position", *pos_offsets, -3.0f, 3.0f);
         ImGui::SliderFloat3("Scale", *scale_offsets, -2.0f, 2.0f);
         ImGui::SliderFloat3("Rotation", *rot_offsets, -180.0f, 180.0f);
        ImGui::End();

        ImGui::Begin("Light position");
            ImGui::SliderAngle("theta", &light_position.theta, 0.0f, 360.0f);
            ImGui::SliderAngle("phi", &light_position.phi, 0.0f, 180.0f);
        ImGui::End();

        ImGui::Begin("Chart Manipulation");
            ImGui::SliderFloat("Time", &timer, 0.0f, 20.0f);
            ImGui::SliderFloat("Chart Multiplier", &multiplier, 0.05f, 10.0f);
            ImGui::SliderFloat("Framerate", &framerate, 0.1f, 60.0f);
            ImGui::Checkbox("Running Timer", &timerrunning);
            ImGui::Checkbox("Pokaż fazę", &colored);

        ImGui::End();

        ImGui::SFML::Render(window);

        window.display();
    }
    ImGui::SFML::Shutdown();
    return 0;
}
