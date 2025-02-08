#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <SFML/Audio/SoundBuffer.hpp>
#include <utilities/shader.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <utilities/timeutils.h>
#include <utilities/mesh.h>
#include <utilities/shapes.h>
#include <utilities/glutils.h>
#include <utilities/imageLoader.hpp>
#include <SFML/Audio/Sound.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include "gamelogic.h"
#include "fmt/core.h"
#include "sceneGraph.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp> // Enables to_string on glm types, handy for debugging

#include "utilities/imageLoader.hpp"
#include "utilities/glfont.h"

enum KeyFrameAction {
    BOTTOM, TOP
};

#include <timestamps.h>

double padPositionX = 0;
double padPositionZ = 0;

unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;

SceneNode* rootNode;
SceneNode* boxNode;
SceneNode* ballNode;
SceneNode* padNode;

double ballRadius = 3.0f;

// These are heap allocated, because they should not be initialised at the start of the program
sf::SoundBuffer* buffer;
Gloom::Shader* shader3D;
Gloom::Shader* shader2D;
sf::Sound* sound;

const glm::vec3 boxDimensions(180, 90, 90);
const glm::vec3 padDimensions(30, 3, 40);

glm::vec3 ballPosition(0, ballRadius + padDimensions.y, boxDimensions.z / 2);
glm::vec3 ballDirection(1, 1, 0.2f);

CommandLineOptions options;

bool hasStarted        = false;
bool hasLost           = false;
bool jumpedToNextFrame = false;
bool isPaused          = false;

bool mouseLeftPressed   = false;
bool mouseLeftReleased  = false;
bool mouseRightPressed  = false;
bool mouseRightReleased = false;

// Modify if you want the music to start further on in the track. Measured in seconds.
const float debug_startTime = 0;
double totalElapsedTime = debug_startTime;
double gameElapsedTime = debug_startTime;

double mouseSensitivity = 1.0;
double lastMouseX = windowWidth / 2;
double lastMouseY = windowHeight / 2;


#define SHADER_CAMERA_LOCATION 6
glm::vec3 cameraPosition = glm::vec3(0, 2, -20);

#define LIGHT_SOURCES 3
SceneNode *lightSources[LIGHT_SOURCES];

unsigned int charMapTextureID;

void mouseCallback(GLFWwindow* window, double x, double y) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;

    padPositionX -= mouseSensitivity * deltaX / windowWidth;
    padPositionZ -= mouseSensitivity * deltaY / windowHeight;

    if (padPositionX > 1) padPositionX = 1;
    if (padPositionX < 0) padPositionX = 0;
    if (padPositionZ > 1) padPositionZ = 1;
    if (padPositionZ < 0) padPositionZ = 0;

    glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
}


// @TODO: move into own file?
unsigned int imageToTexture(PNGImage image) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Copy image into texture
    glTexImage2D(GL_TEXTURE_2D,
                 0, // Level of detail. 0 since we are only creating a single texture.
                 GL_RGBA, // Internal format
                 image.width, // Width
                 image.height, // Height
                 0, // Border
                 GL_RGBA, // Input format the input data is stored in
                 GL_UNSIGNED_BYTE, // Data and type per channel
                 image.pixels.data()); // Image data

    glGenerateMipmap(GL_TEXTURE_2D);
    // Configure the sampling options for the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return textureID;
}

void initGame(GLFWwindow* window, CommandLineOptions gameOptions) {
    PNGImage charMap = loadPNGFile("../res/textures/charmap.png");
    charMapTextureID = imageToTexture(charMap);

    Mesh helloMomText = generateTextGeometryBuffer("Hello Mom !", 39.0 / 29.0, 500.0);
    unsigned int textVAO = generateBuffer(helloMomText);
    SceneNode *textNode = createSceneNode(GEOMETRY_2D);
    textNode->vertexArrayObjectID = textVAO;
    textNode->textureID = charMapTextureID;
    textNode->VAOIndexCount = helloMomText.indices.size();
    textNode->position  = { 0, 0, 0 };

    buffer = new sf::SoundBuffer();
    if (!buffer->loadFromFile("../res/Hall of the Mountain King.ogg")) {
        return;
    }

    options = gameOptions;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    shader3D = new Gloom::Shader();
    shader3D->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    shader3D->activate();

    shader2D = new Gloom::Shader();
    shader2D->makeBasicShader("../res/shaders/2d.vert", "../res/shaders/2d.frag");

    // Create meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);
    Mesh sphere = generateSphere(1.0, 40, 40);

    // Fill buffers
    unsigned int ballVAO = generateBuffer(sphere);
    unsigned int boxVAO  = generateBuffer(box);
    unsigned int padVAO  = generateBuffer(pad);

    // Construct scene
    rootNode = createSceneNode(GEOMETRY);
    boxNode  = createSceneNode(GEOMETRY);
    padNode  = createSceneNode(GEOMETRY);
    ballNode = createSceneNode(GEOMETRY);

    rootNode->children.push_back(boxNode);
    rootNode->children.push_back(padNode);
    rootNode->children.push_back(ballNode);
    rootNode->children.push_back(textNode);

    boxNode->vertexArrayObjectID  = boxVAO;
    boxNode->VAOIndexCount        = box.indices.size();

    padNode->vertexArrayObjectID  = padVAO;
    padNode->VAOIndexCount        = pad.indices.size();

    ballNode->vertexArrayObjectID = ballVAO;
    ballNode->VAOIndexCount       = sphere.indices.size();

    // 2D Geometry root node
    // Add lights
    SceneNode *padLight = createSceneNode(POINT_LIGHT);
    padLight->position = glm::vec3(-5.0, 5.0, 20.0);
    padLight->lightColor = glm::vec3(1.0, 0.0, 0.0);
    padNode->children.push_back(padLight);
    lightSources[padLight->lightNodeID] = padLight;
    SceneNode *padLight2 = createSceneNode(POINT_LIGHT);
    padLight2->position = glm::vec3(0.0, 5.0, 20.0);
    padLight2->lightColor = glm::vec3(0.0, 1.0, 0.0);
    padNode->children.push_back(padLight2);
    lightSources[padLight2->lightNodeID] = padLight2;
    SceneNode *padLight3 = createSceneNode(POINT_LIGHT);
    padLight3->position = glm::vec3(5.0, 5.0, 20.0);
    padLight3->lightColor = glm::vec3(0.0, 0.0, 1.0);
    padNode->children.push_back(padLight3);
    lightSources[padLight3->lightNodeID] = padLight3;

    //SceneNode *roofLightLeft = createSceneNode(POINT_LIGHT);
    //roofLightLeft->position = glm::vec3(-80, 30, 10);
    //roofLightLeft->lightColor = glm::vec3(1.0, 0.0, 0.0);
    //boxNode->children.push_back(roofLightLeft);
    //lightSources[roofLightLeft->lightNodeID] = roofLightLeft;

    //SceneNode *roofLightRight = createSceneNode(POINT_LIGHT);
    //roofLightRight->position = glm::vec3(80, 30, 10);
    //roofLightRight->lightColor = glm::vec3(0.0, 1.0, 0.0);
    //boxNode->children.push_back(roofLightRight);
    //lightSources[roofLightRight->lightNodeID] = roofLightRight;
    

    getTimeDeltaSeconds();

    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;
    std::cout << "Ready. Click to start!" << std::endl;
}

static void updateGameLogic(GLFWwindow *window) {
    double timeDelta = getTimeDeltaSeconds();

    const float ballBottomY = boxNode->position.y - (boxDimensions.y/2) + ballRadius + padDimensions.y;
    const float ballTopY    = boxNode->position.y + (boxDimensions.y/2) - ballRadius;
    const float BallVerticalTravelDistance = ballTopY - ballBottomY;

    const float cameraWallOffset = 30; // Arbitrary addition to prevent ball from going too much into camera

    const float ballMinX = boxNode->position.x - (boxDimensions.x/2) + ballRadius;
    const float ballMaxX = boxNode->position.x + (boxDimensions.x/2) - ballRadius;
    const float ballMinZ = boxNode->position.z - (boxDimensions.z/2) + ballRadius;
    const float ballMaxZ = boxNode->position.z + (boxDimensions.z/2) - ballRadius - cameraWallOffset;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
        mouseLeftPressed = true;
        mouseLeftReleased = false;
    } else {
        mouseLeftReleased = mouseLeftPressed;
        mouseLeftPressed = false;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2)) {
        mouseRightPressed = true;
        mouseRightReleased = false;
    } else {
        mouseRightReleased = mouseRightPressed;
        mouseRightPressed = false;
    }

    if(!hasStarted) {
        if (mouseLeftPressed) {
            if (options.enableMusic) {
                sound = new sf::Sound();
                sound->setBuffer(*buffer);
                sf::Time startTime = sf::seconds(debug_startTime);
                sound->setPlayingOffset(startTime);
                sound->play();
            }
            totalElapsedTime = debug_startTime;
            gameElapsedTime = debug_startTime;
            hasStarted = true;
        }

        ballPosition.x = ballMinX + (1 - padPositionX) * (ballMaxX - ballMinX);
        ballPosition.y = ballBottomY;
        ballPosition.z = ballMinZ + (1 - padPositionZ) * ((ballMaxZ+cameraWallOffset) - ballMinZ);
    } else {
        totalElapsedTime += timeDelta;
        if(hasLost) {
            if (mouseLeftReleased) {
                hasLost = false;
                hasStarted = false;
                currentKeyFrame = 0;
                previousKeyFrame = 0;
            }
        } else if (isPaused) {
            if (mouseRightReleased) {
                isPaused = false;
                if (options.enableMusic) {
                    sound->play();
                }
            }
        } else {
            gameElapsedTime += timeDelta;
            if (mouseRightReleased) {
                isPaused = true;
                if (options.enableMusic) {
                    sound->pause();
                }
            }
            // Get the timing for the beat of the song
            for (unsigned int i = currentKeyFrame; i < keyFrameTimeStamps.size(); i++) {
                if (gameElapsedTime < keyFrameTimeStamps.at(i)) {
                    continue;
                }
                currentKeyFrame = i;
            }

            jumpedToNextFrame = currentKeyFrame != previousKeyFrame;
            previousKeyFrame = currentKeyFrame;

            double frameStart = keyFrameTimeStamps.at(currentKeyFrame);
            double frameEnd = keyFrameTimeStamps.at(currentKeyFrame + 1); // Assumes last keyframe at infinity

            double elapsedTimeInFrame = gameElapsedTime - frameStart;
            double frameDuration = frameEnd - frameStart;
            double fractionFrameComplete = elapsedTimeInFrame / frameDuration;

            double ballYCoord;

            KeyFrameAction currentOrigin = keyFrameDirections.at(currentKeyFrame);
            KeyFrameAction currentDestination = keyFrameDirections.at(currentKeyFrame + 1);

            // Synchronize ball with music
            if (currentOrigin == BOTTOM && currentDestination == BOTTOM) {
                ballYCoord = ballBottomY;
            } else if (currentOrigin == TOP && currentDestination == TOP) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance;
            } else if (currentDestination == BOTTOM) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance * (1 - fractionFrameComplete);
            } else if (currentDestination == TOP) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance * fractionFrameComplete;
            }

            // Make ball move
            const float ballSpeed = 60.0f;
            ballPosition.x += timeDelta * ballSpeed * ballDirection.x;
            ballPosition.y = ballYCoord;
            ballPosition.z += timeDelta * ballSpeed * ballDirection.z;

            // Make ball bounce
            if (ballPosition.x < ballMinX) {
                ballPosition.x = ballMinX;
                ballDirection.x *= -1;
            } else if (ballPosition.x > ballMaxX) {
                ballPosition.x = ballMaxX;
                ballDirection.x *= -1;
            }
            if (ballPosition.z < ballMinZ) {
                ballPosition.z = ballMinZ;
                ballDirection.z *= -1;
            } else if (ballPosition.z > ballMaxZ) {
                ballPosition.z = ballMaxZ;
                ballDirection.z *= -1;
            }

            if(options.enableAutoplay) {
                padPositionX = 1-(ballPosition.x - ballMinX) / (ballMaxX - ballMinX);
                padPositionZ = 1-(ballPosition.z - ballMinZ) / ((ballMaxZ+cameraWallOffset) - ballMinZ);
            }

            // Check if the ball is hitting the pad when the ball is at the bottom.
            // If not, you just lost the game! (hehe)
            if (jumpedToNextFrame && currentOrigin == BOTTOM && currentDestination == TOP) {
                double padLeftX  = boxNode->position.x - (boxDimensions.x/2) + (1 - padPositionX) * (boxDimensions.x - padDimensions.x);
                double padRightX = padLeftX + padDimensions.x;
                double padFrontZ = boxNode->position.z - (boxDimensions.z/2) + (1 - padPositionZ) * (boxDimensions.z - padDimensions.z);
                double padBackZ  = padFrontZ + padDimensions.z;

                if (   ballPosition.x < padLeftX
                    || ballPosition.x > padRightX
                    || ballPosition.z < padFrontZ
                    || ballPosition.z > padBackZ
                ) {
                    hasLost = true;
                    if (options.enableMusic) {
                        sound->stop();
                        delete sound;
                    }
                }
            }
        }
    }

    // Move and rotate various SceneNodes
    boxNode->position = { 0, -10, -80 };

    ballNode->position = ballPosition;
    ballNode->scale = glm::vec3(ballRadius);
    ballNode->rotation = { 0, totalElapsedTime*2, 0 };

    padNode->position  = {
        boxNode->position.x - (boxDimensions.x/2) + (padDimensions.x/2) + (1 - padPositionX) * (boxDimensions.x - padDimensions.x),
        boxNode->position.y - (boxDimensions.y/2) + (padDimensions.y/2),
        boxNode->position.z - (boxDimensions.z/2) + (padDimensions.z/2) + (1 - padPositionZ) * (boxDimensions.z - padDimensions.z)
    };
}

void updateFrame(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    updateGameLogic(window);

    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(windowWidth) / float(windowHeight), 0.1f, 350.f);

    // Some math to make the camera move in a nice way
    float lookRotation = -0.6 / (1 + exp(-5 * (padPositionX-0.5))) + 0.3;
    glm::mat4 cameraTransform =
                    glm::rotate(0.3f + 0.2f * float(-padPositionZ*padPositionZ), glm::vec3(1, 0, 0)) *
                    glm::rotate(lookRotation, glm::vec3(0, 1, 0)) *
                    glm::translate(-cameraPosition);

    glm::mat4 VP = projection * cameraTransform;

    glm::mat4 identity = glm::mat4(1);
    updateNodeTransformations(rootNode, identity, VP);
}

void updateNodeTransformations(SceneNode* node, glm::mat4 transformationThusFar, glm::mat4 VP) {
    glm::mat4 transformationMatrix =
              glm::translate(node->position)
            * glm::translate(node->referencePoint)
            * glm::rotate(node->rotation.y, glm::vec3(0,1,0))
            * glm::rotate(node->rotation.x, glm::vec3(1,0,0))
            * glm::rotate(node->rotation.z, glm::vec3(0,0,1))
            * glm::scale(node->scale)
            * glm::translate(-node->referencePoint);

    node->modelMatrix = transformationThusFar * transformationMatrix;
    node->currentTransformationMatrix = VP * node->modelMatrix;
    node->normalMatrix = glm::mat3(glm::transpose(glm::inverse(node->modelMatrix)));

    switch(node->nodeType) {
        case GEOMETRY_2D: break;
        case GEOMETRY: break;
        case POINT_LIGHT:
            node->lightPosition = glm::vec3(node->modelMatrix * glm::vec4(0, 0, 0, 1));
            break;
        case SPOT_LIGHT: {
        } break;
    }

    for(SceneNode* child : node->children) {
        updateNodeTransformations(child, node->modelMatrix, VP);
    }
}


void renderNode3D(SceneNode* node) {
    glUniformMatrix4fv(3, 1, GL_FALSE, glm::value_ptr(node->currentTransformationMatrix));
    glUniformMatrix4fv(4, 1, GL_FALSE, glm::value_ptr(node->modelMatrix));
    glUniformMatrix3fv(5, 1, GL_FALSE, glm::value_ptr(node->normalMatrix));

    switch(node->nodeType) {
        case GEOMETRY:
            if(node->vertexArrayObjectID != -1) {
                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
        case GEOMETRY_2D: {
        } break;
        case POINT_LIGHT: {
        } break;
        case SPOT_LIGHT: {
        } break;
    }

    for(SceneNode* child : node->children) {
        renderNode3D(child);
    }
}

void render3D(SceneNode *root) {
    shader3D->activate();
    glUniform3fv(SHADER_CAMERA_LOCATION, 1, glm::value_ptr(cameraPosition));

    // Pass light positions to fragment shader
    for (int i = 0; i < LIGHT_SOURCES; i++) {
        SceneNode *node = lightSources[i];
        auto prefix = fmt::format("light_sources[{}]", i);
        // Position
        auto location_position = shader3D->getUniformFromName(prefix + ".position");
        glUniform3fv(location_position, 1, glm::value_ptr(node->lightPosition));
        // Color
        auto location_color = shader3D->getUniformFromName(prefix + ".color");
        glUniform3fv(location_color, 1, glm::value_ptr(node->lightColor));
    }

    // Pass ball position to fragment shader
    glUniform3fv(shader3D->getUniformFromName("ball_position"), 1, glm::value_ptr(ballNode->position));

    renderNode3D(root);
}

void renderNode2D(SceneNode* node) {
    switch(node->nodeType) {
        case GEOMETRY_2D: {
            if (node->vertexArrayObjectID != -1) {
                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
        default: break;
        }
    }

    for(SceneNode* child : node->children) {
        renderNode2D(child);
    }
}

void render2D(SceneNode *root) {
    shader2D->activate();
    /* Orthographic project with center (0,0) at the bottom left corner */
    glm::mat4 orthographicProjection = glm::ortho(0.0f,
                                                  (float)windowWidth,
                                                  0.0f,
                                                  (float)windowHeight,
                                                  0.0f, // Near plane
                                                  1.0f); // Far plane
    glUniformMatrix4fv(3, 1, GL_FALSE, glm::value_ptr(orthographicProjection));

    glBindTextureUnit(0, charMapTextureID);
    renderNode2D(root);
}

void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    render3D(rootNode);
    render2D(rootNode);
}
