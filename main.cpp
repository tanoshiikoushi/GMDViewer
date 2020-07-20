#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

struct MouseData
{
    bool lActive = false;
    bool rActive = false;
    bool mActive = false;
    float mouseVDir = 0.0f;
};

struct CameraData
{
    glm::vec3 camPos;
    glm::vec3 lookAtPos;
    float xRot, yRot, zRot;
    float vPov;
    float nearPlane, farPlane;
};

struct DisplayListInitialization
{
    float x_size, y_size, z_size; //maybe not necessary... we'll check
    float x_origin, y_origin, z_origin;
    std::string layer_name = "";
    unsigned short layer_id;
};

struct ModelData
{
    unsigned short materialCount = 0;
    char* vertexData;
    unsigned long vertexDataSize;
    char** displayList;
    DisplayListInitialization* displayListInit;
    unsigned long* displayListSize;
    SDL_Surface** texture;
    unsigned int* texture_obj;
    bool* drawDisplayList;
    unsigned short displayListCount = 0;
};

unsigned int modelStride = 0x1C;
unsigned int dListVtxOffset = 0x6; //6 bytes skipped after we read the vtx
double scaleFactor = 0.02f;
float global_x_size, global_y_size, global_z_size;

float scrollMultiply = 1.f;
float movementMultiply = 1.f;

SDL_Window* window = NULL;
SDL_GLContext gContext;
bool quit = false;
float mouse_to_degree = 0.5f;
float mouse_movement = 0.02f; //0.02f;
float mouse_scroll = 0.2f; //0.2f;
MouseData mouse;
CameraData camera;

bool printed = false;

std::string base_temp = "F:/Ty RE/rkv/model_extract/_PP_Files/";

//thanks stackoverflow :)
float bintofloat(unsigned int x) {
    union {
        unsigned int  x;
        float  f;
    } temp;
    temp.x = x;
    return temp.f;
}

unsigned short fractionalCount = 12;
double fixedtodouble(unsigned short f)
{
    //AAA gotta figure out texcoord generation @w@
    double ret = 0;
    return ret;
}

void loadModel (char* file_path, ModelData* model)
{
    std::fstream in_file;
    in_file.open(file_path, std::ios::in | std::ios::binary);

    char* in_buf = new char[4];
    //Check Magic
    in_file.read(in_buf, 4);
    if (!(in_buf[0] == 'M' && in_buf[1] == 'D' && in_buf[2] == 'L' && in_buf[3] == '2'))
    {
        exit(-1);
    }

    //read material count, maybe scale?
    in_file.read(in_buf, 2);
    {
        model->materialCount = ((in_buf[0]) & 0xFF) << 8 | ((in_buf[1]) & 0xFF);
    }

    //read display list count
    in_file.read(in_buf, 2);
    {
        model->displayListCount = ((in_buf[0]) & 0xFF) << 8 | ((in_buf[1]) & 0xFF);
    }

    in_file.seekg(0x20, std::ios::beg);
    in_file.read(in_buf, 4);
    in_file.read(in_buf, 4);
    in_file.read(in_buf, 4);
    in_file.read(in_buf, 4); //padding

    //read and calculate global ratios
    in_file.read(in_buf, 4);
    global_x_size = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
    in_file.read(in_buf, 4);
    global_y_size = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
    in_file.read(in_buf, 4);
    global_z_size = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));

    printf("Model Dimensions x: %f y: %f z: %f\n\n", global_x_size, global_y_size, global_z_size);

    //read display lists
    in_file.seekg(0x0C, std::ios::beg);
    in_file.read(in_buf, 4);
    unsigned long displayPointer = (((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
    unsigned long matPointer = 0;
    unsigned short matStrLength = 0;

    in_file.read(in_buf, 4); //unk
    in_file.read(in_buf, 4);
    unsigned long transform_pointer = (((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
    unsigned long transform_size = 0x10;

    model->displayList = new char*[model->displayListCount];
    model->displayListSize = new unsigned long[model->displayListCount];
    model->displayListInit = new DisplayListInitialization[model->displayListCount];
    model->drawDisplayList = new bool[model->displayListCount];
    model->texture = new SDL_Surface*[model->displayListCount];
    model->texture_obj = new unsigned int[model->displayListCount];
    glGenTextures(model->displayListCount, model->texture_obj);

    unsigned long displayListPointer = 0, displayListHeaderPointer = 0;

    //read vertex data
    in_file.seekg(0x18, std::ios::beg);
    in_file.read(in_buf, 4);
    unsigned long tempVTXData = ((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF);

    in_file.read(in_buf, 4);
    model->vertexDataSize = (((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF)) * modelStride;
    model->vertexData = (char*)malloc(model->vertexDataSize);

    in_file.seekg(tempVTXData, std::ios::beg);
    in_file.read(model->vertexData, model->vertexDataSize);

    //display list data
    for (unsigned long d = 0; d < model->displayListCount; d++)
    {
        in_file.seekg(displayPointer, std::ios::beg);

        in_file.read(in_buf, 4);
        in_file.read(in_buf, 4);
        in_file.read(in_buf, 4);
        in_file.read(in_buf, 4); //padding

        in_file.read(in_buf, 4);
        model->displayListInit[d].x_size = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
        in_file.read(in_buf, 4);
        model->displayListInit[d].y_size = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
        in_file.read(in_buf, 4);
        model->displayListInit[d].z_size = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
        in_file.read(in_buf, 4); //padding

        in_file.read(in_buf, 4);
        model->displayListInit[d].x_origin = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
        in_file.read(in_buf, 4);
        model->displayListInit[d].y_origin = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
        in_file.read(in_buf, 4);
        model->displayListInit[d].z_origin = bintofloat(((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF));
        in_file.read(in_buf, 4); //padding

        //0x30-0x3F are padding
        in_file.read(in_buf, 4);
        unsigned long layerNamePointer = ((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF);
        in_file.read(in_buf, 4); //junk
        in_file.read(in_buf, 4); //yes
        unsigned long vCountUnk = ((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF);
        in_file.read(in_buf, 4);

        //two unks
        in_file.read(in_buf, 4);

        in_file.read(in_buf, 4);
        displayListHeaderPointer = ((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF);

        in_file.seekg(layerNamePointer, std::ios::beg);
        char c_in = 0;
        do
        {
            in_file.read(&c_in, 1);
            if (c_in != 0x00)
            {
                model->displayListInit[d].layer_name += c_in;
            }
        } while (c_in != 0x00);

        in_file.seekg(displayListHeaderPointer, std::ios::beg);
        in_file.read(in_buf, 4); //material pointer
        matPointer = ((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF);
        in_file.seekg(matPointer, std::ios::beg);
        char currR = 0;
        std::string matStr;
        do
        {
            in_file.read(&currR, 1);
            if (currR!= 0x00)
            {
                matStr += currR;
            }
        } while (currR != 0x00);
        matStr.append(".bmp");
        printf("%s\n", matStr.c_str());
        model->texture[d] = SDL_LoadBMP((base_temp + matStr).c_str());
        if (model->texture[d] == NULL)
        {
            printf("Error loading material '%s': %s\n", (base_temp + matStr).c_str(), SDL_GetError());
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, model->texture_obj[d]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, model->texture[d]->w, model->texture[d]->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, model->texture[d]->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        in_file.seekg(displayListHeaderPointer + 0x4, std::ios::beg);
        in_file.read(in_buf, 4); //display list pointer
        displayListPointer = ((in_buf[0]) & 0xFF) << 24 | ((in_buf[1]) & 0xFF) << 16 | ((in_buf[2]) & 0xFF) << 8 | ((in_buf[3]) & 0xFF);
        in_file.clear();

        in_file.read(in_buf, 2);
        model->displayListSize[d] = (((in_buf[0] & 0xFF) << 8) | (in_buf[1] & 0xFF)) << 4;
        model->displayList[d] = (char*)malloc(model->displayListSize[d]);

        in_file.seekg(displayListPointer, std::ios::beg);
        in_file.read(model->displayList[d], model->displayListSize[d]);

        printf("Display List #%lu: %s\n", d, model->displayListInit[d].layer_name.c_str());
        printf("Display List #%lu: 0x%lX - Origin x: %f y: %f z: %f\n", d, displayListPointer, model->displayListInit[d].x_origin, model->displayListInit[d].y_origin, model->displayListInit[d].z_origin);
        printf("Display List #%lu: 0x%lX - Size x: %f y: %f z: %f\n", d, displayListPointer, model->displayListInit[d].x_size, model->displayListInit[d].y_size, model->displayListInit[d].z_size);

        model->drawDisplayList[d] = true;
        displayPointer += 0x50;
    }

    delete[] in_buf;
    return;
}

void unloadModel(ModelData* model)
{
    model->vertexDataSize = 0;
    free(model->vertexData);
    model->vertexData = nullptr;

    for (unsigned short dListInd = 0; dListInd < model->displayListCount; dListInd++)
    {
        free(model->displayList[dListInd]);
        model->displayList[dListInd] = nullptr;
        model->displayListSize[dListInd] = 0;
        SDL_FreeSurface(model->texture[dListInd]);
        model->texture[dListInd] = nullptr;
        model->texture_obj[dListInd] = 0;
    }

    delete[] model->displayListInit;
    delete[] model->displayListSize;
    delete[] model->drawDisplayList;

    model->materialCount = 0;
    model->displayListCount = 0;

    global_x_size = 0;
    global_y_size = 0;
    global_z_size = 0;
}

void mouse_handler(SDL_Event* event)
{
    if (event->type == SDL_MOUSEBUTTONDOWN)
    {
        switch (event->button.button)
        {
        case SDL_BUTTON_LEFT:
            mouse.lActive = true;
            if (camera.xRot < 0)
            {
                mouse.mouseVDir = -1.0f;
            }
            else
            {
                mouse.mouseVDir = 1.0f;
            }
            break;
        case SDL_BUTTON_RIGHT:
            mouse.rActive = true;
            break;
        case SDL_BUTTON_MIDDLE:
            mouse.mActive = true;
            break;
        }
    }
    else if (event->type == SDL_MOUSEBUTTONUP)
    {
        switch (event->button.button)
        {
        case SDL_BUTTON_LEFT:
            mouse.lActive = false;
            mouse.mouseVDir = 0.0f;
            break;
        case SDL_BUTTON_RIGHT:
            mouse.rActive = false;
            break;
        case SDL_BUTTON_MIDDLE:
            mouse.mActive = false;
            break;
        }
    }
    else if (event->type == SDL_MOUSEMOTION)
    {
        if (mouse.lActive)
        {
            int relX = event->motion.xrel;
            int relY = event->motion.yrel;

            camera.xRot += relY * mouse_to_degree;
            camera.yRot += relX * mouse_to_degree * mouse.mouseVDir;
        }
        else if (mouse.mActive)
        {
            int relX = event->motion.xrel;
            camera.zRot += relX * mouse_to_degree * -1;
        }
        else if (mouse.rActive)
        {
            int relX = event->motion.xrel;
            int relY = event->motion.yrel;
            glm::vec3 facing = camera.lookAtPos - camera.camPos;
            facing = glm::normalize(facing);

            //move up/down and left/right
            if (relX < 0)
            {
                glm::vec3 left = glm::cross(glm::vec3(0, 1, 0), facing);
                camera.camPos += left * (-1 * relX * mouse_movement * movementMultiply);
                camera.lookAtPos += left * (-1 * relX * mouse_movement * movementMultiply);
            }
            else if (relX > 0)
            {
                glm::vec3 right = glm::cross(facing, glm::vec3(0, 1, 0));
                camera.camPos += right * (relX * mouse_movement * movementMultiply);
                camera.lookAtPos += right * (relX * mouse_movement * movementMultiply);
            }

            if (relY < 0)
            {
                glm::vec3 left = glm::cross(glm::vec3(0, 1, 0), facing);
                glm::vec3 up = glm::cross(facing, left);
                camera.camPos += up * (-1 * relY * mouse_movement * movementMultiply);
                camera.lookAtPos += up * (-1 * relY * mouse_movement * movementMultiply);
            }
            else if (relY > 0)
            {
                glm::vec3 left = glm::cross(glm::vec3(0, 1, 0), facing);
                glm::vec3 down = glm::cross(left, facing);
                camera.camPos += down * (relY * mouse_movement * movementMultiply);
                camera.lookAtPos += down * (relY * mouse_movement * movementMultiply);
            }
        }
    }
    else if (event->type == SDL_MOUSEWHEEL)
    {
        //Positive moves closer, negative moves farther
        glm::vec3 facing = camera.lookAtPos - camera.camPos;
        facing = glm::normalize(facing);
        camera.camPos += facing * (event->wheel.y * mouse_scroll * scrollMultiply);
        camera.lookAtPos += facing * (event->wheel.y * mouse_scroll * scrollMultiply);
    }
}

void cameraReset()
{
    camera.camPos = glm::vec3(0.f, 0.f, -5.f); //0, 0, 10
    camera.lookAtPos = glm::vec3(0.f, 0.f, 0.f); //0, 0, 0
    camera.xRot = 0;
    camera.yRot = 0;
    camera.zRot = 0;
    camera.vPov = 45;
    camera.nearPlane = 0.1f;
    camera.farPlane = 100000000000.f;
}


void keyboard_handler(SDL_Keycode key, ModelData* model)
{
    switch (key)
    {
    case SDLK_ESCAPE:
        quit = true;
        break;
    case SDLK_r:
        cameraReset();
        break;
    case SDLK_LSHIFT:
        if (scrollMultiply == 100.0f) { scrollMultiply = 1.0f; }
        else { scrollMultiply = 100.0f; }
        if (movementMultiply == 100.0f) { movementMultiply = 1.0f; }
        else { movementMultiply = 100.0f; }
        break;
    case SDLK_0:
        model->drawDisplayList[0] = !(model->drawDisplayList[0]);
        break;
    case SDLK_1:
        model->drawDisplayList[1] = !(model->drawDisplayList[1]);
        break;
    case SDLK_2:
        model->drawDisplayList[2] = !(model->drawDisplayList[2]);
        break;
    case SDLK_3:
        model->drawDisplayList[3] = !(model->drawDisplayList[3]);
        break;
    case SDLK_4:
        model->drawDisplayList[4] = !(model->drawDisplayList[4]);
        break;
    case SDLK_5:
        model->drawDisplayList[5] = !(model->drawDisplayList[5]);
        break;
    case SDLK_6:
        model->drawDisplayList[6] = !(model->drawDisplayList[6]);
        break;
    case SDLK_7:
        model->drawDisplayList[7] = !(model->drawDisplayList[7]);
        break;
    case SDLK_8:
        model->drawDisplayList[8] = !(model->drawDisplayList[8]);
        break;
    case SDLK_9:
        model->drawDisplayList[9] = !(model->drawDisplayList[9]);
        break;
    }
    return;
}

void updateProjectionMatrix()
{
    if (camera.xRot >= 360)
    {
        camera.xRot -= 360;
    }
    else if (camera.xRot <= -360)
    {
        camera.xRot += 360;
    }

    if (camera.yRot >= 360)
    {
        camera.yRot -= 360;
    }
    else if (camera.yRot <= -360)
    {
        camera.yRot += 360;
    }

    if (camera.zRot >= 360)
    {
        camera.zRot -= 360;
    }
    else if (camera.zRot <= -360)
    {
        camera.zRot += 360;
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(camera.vPov, 1, camera.nearPlane, camera.farPlane);
    gluLookAt(camera.camPos.x, camera.camPos.y, camera.camPos.z, camera.lookAtPos.x, camera.lookAtPos.y, camera.lookAtPos.z, 0, 1, 0);
    glRotatef(camera.xRot, 1, 0, 0);
    glRotatef(camera.yRot, 0, 1, 0);
    glRotatef(camera.zRot, 0, 0, 1);
}

bool glInit()
{
    bool success = true;
    GLenum error = GL_NO_ERROR;

    cameraReset();

    glFrontFace(GL_CW);
    glPolygonOffset(1,1);

    updateProjectionMatrix();

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf( "Error initializing OpenGL. Error: %s\n", gluErrorString( error ) );
        success = false;
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf( "Error initializing OpenGL. Error: %s\n", gluErrorString( error ) );
        success = false;
    }

    glClearColor(0.f, 0.f, 0.f, 1.f);

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf( "Error initializing OpenGL. Error: %s\n", gluErrorString( error ) );
        success = false;
    }

    return success;
}

float min_x = 0, min_y = 0, min_z = 0, max_x = 0, max_y = 0, max_z = 0;

void drawTriangleStrip(ModelData* model, unsigned long &ind, unsigned short dListInd)
{
    unsigned short vtxCount = ((model->displayList[dListInd][ind] & 0xFF) << 8) | (model->displayList[dListInd][ind+1] & 0xFF);
    ind += 2;

    unsigned long orig_ind = ind;

    unsigned short currVert = 0;
    float x, y, z;
    double s, t;

    glPointSize(1.f);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glColor3f(0.560f, 0.560f, 0.560f);
    glBegin(GL_TRIANGLE_STRIP);
    for (unsigned short i = 0; i < vtxCount; i++)
    {
        currVert = ((model->displayList[dListInd][ind] & 0xFF) << 8) | (model->displayList[dListInd][ind+1] & 0xFF);
        ind += 2;
        if (currVert == 0xFFFF) { continue; }

        x = bintofloat(((model->vertexData[currVert * 0x1C] & 0xFF) << 24) | ((model->vertexData[currVert * 0x1C + 1] & 0xFF) << 16) | ((model->vertexData[currVert * 0x1C + 2] & 0xFF) << 8)
            | ((model->vertexData[currVert * 0x1C + 3] & 0xFF)));
        y = bintofloat(((model->vertexData[currVert * 0x1C + 4] & 0xFF) << 24) | ((model->vertexData[currVert * 0x1C + 4 + 1] & 0xFF) << 16) | ((model->vertexData[currVert * 0x1C + 4 + 2] & 0xFF) << 8)
            | ((model->vertexData[currVert * 0x1C + 4 + 3] & 0xFF)));
        z = bintofloat(((model->vertexData[currVert * 0x1C + 8] & 0xFF) << 24) | ((model->vertexData[currVert * 0x1C + 8 + 1] & 0xFF) << 16) | ((model->vertexData[currVert * 0x1C + 8 + 2] & 0xFF) << 8)
            | ((model->vertexData[currVert * 0x1C + 8 + 3] & 0xFF)));

        /*s = fixedtodouble(((model->vertexData[currVert * 0x1C + 0x13] & 0xFF) << 8) | ((model->vertexData[currVert * 0x1C + 0x14] & 0xFF)));
        t = fixedtodouble(((model->vertexData[currVert * 0x1C + 0x15] & 0xFF) << 8) | ((model->vertexData[currVert * 0x1C + 0x16] & 0xFF)));
        glTexCoord2d(s, t);
        if (!printed)
        {
            printf("s: %f t: %f\n", s, t);
        }*/
        glVertex3f(x, y, z);

        ind += dListVtxOffset;
    }
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);

    //now do the same thing again but this time we draw the lines
    ind = orig_ind;
    glColor3f(0.f, 0.f, 0.f);
    glBegin(GL_LINE_LOOP);
    for (unsigned short i = 0; i < vtxCount; i++)
    {
        currVert = ((model->displayList[dListInd][ind] & 0xFF) << 8) | (model->displayList[dListInd][ind+1] & 0xFF);
        ind += 2;
        if (currVert == 0xFFFF) { continue; }

        x = bintofloat(((model->vertexData[currVert * 0x1C] & 0xFF) << 24) | ((model->vertexData[currVert * 0x1C + 1] & 0xFF) << 16) | ((model->vertexData[currVert * 0x1C + 2] & 0xFF) << 8)
            | ((model->vertexData[currVert * 0x1C + 3] & 0xFF)));
        y = bintofloat(((model->vertexData[currVert * 0x1C + 4] & 0xFF) << 24) | ((model->vertexData[currVert * 0x1C + 4 + 1] & 0xFF) << 16) | ((model->vertexData[currVert * 0x1C + 4 + 2] & 0xFF) << 8)
            | ((model->vertexData[currVert * 0x1C + 4 + 3] & 0xFF)));
        z = bintofloat(((model->vertexData[currVert * 0x1C + 8] & 0xFF) << 24) | ((model->vertexData[currVert * 0x1C + 8 + 1] & 0xFF) << 16) | ((model->vertexData[currVert * 0x1C + 8 + 2] & 0xFF) << 8)
            | ((model->vertexData[currVert * 0x1C + 8 + 3] & 0xFF)));
        glVertex3f(x, y, z);

        ind += dListVtxOffset;
    }
    glEnd();

    glColor3f(0.560f, 0.560f, 0.560f);
    return;
}

void drawModel(ModelData* model)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(-scaleFactor, scaleFactor, scaleFactor); //x_dimension is inverted
    glColor3f(0.560f, 0.560f, 0.560f);

    for (unsigned short dListInd = 0; dListInd < model->displayListCount; dListInd++)
    {
        if (!(model->drawDisplayList[dListInd])) { continue; }

        glPushMatrix();
        glTranslatef(model->displayListInit[dListInd].x_origin, model->displayListInit[dListInd].y_origin, model->displayListInit[dListInd].z_origin);

        if (model->texture[dListInd] != NULL)
        {
            glBindTexture(GL_TEXTURE_2D, model->texture_obj[dListInd]);
            glEnable(GL_TEXTURE_2D);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
        }

        glDisable(GL_TEXTURE_2D);

        for (unsigned long ind = 0; ind < model->displayListSize[dListInd]; )
        {
            unsigned char primitiveType = model->displayList[dListInd][ind];
            ind++;
            switch(primitiveType)
            {
            case 0x00:
                //NOP
                break;
            case 0x98:
                drawTriangleStrip(model, ind, dListInd);
                break;
            default:
                if (!printed) { printf("Unimplemented!\n"); }
                break;
            }
        }

        glPopMatrix();
    }

    glDisable(GL_TEXTURE_2D);
    printed = true;
}

void render(ModelData* model)
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    updateProjectionMatrix();
    drawModel(model);
}

int main (int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Error initializing SDL. Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    window = SDL_CreateWindow("GMD Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 500, 500, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (window == NULL)
    {
        printf("Error initializing window. Error: %s\n", SDL_GetError());
        return 2;
    }

    gContext = SDL_GL_CreateContext(window);
    if (gContext == NULL)
    {
        printf("Error initializing OpenGL Context. Error: %s\n", SDL_GetError());
        return 3;
    }

    //1 = vsync
    if (SDL_GL_SetSwapInterval(1) < 0)
    {
        printf("Warning: Unable to set VSync. Error: %s\n", SDL_GetError());
    }

    SDL_Event handler;
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    ModelData model;

    if (!glInit())
    {
        return 4;
    }

    loadModel("F:/Ty RE/rkv/model_extract/_PP_Files/prop_5x5x1_placeholder.gmd", &model);

    while (!quit)
    {
        while(SDL_PollEvent(&handler) != 0)
        {
            if (handler.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (handler.type == SDL_KEYDOWN)
            {
                keyboard_handler(handler.key.keysym.sym, &model);
            }
            else if (handler.type == SDL_MOUSEMOTION || handler.type == SDL_MOUSEBUTTONDOWN || handler.type == SDL_MOUSEBUTTONUP || handler.type == SDL_MOUSEWHEEL)
            {
                mouse_handler(&handler);
            }
            else if (handler.type == SDL_DROPFILE)
            {
                char* new_file = handler.drop.file;
                unloadModel(&model);
                loadModel(new_file, &model);
                cameraReset();
                SDL_free(new_file);
            }
        }
        render(&model);
        SDL_GL_SwapWindow(window);
    }

    unloadModel(&model);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
