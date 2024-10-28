#include "Renderer.h"
#include "GL/glew.h"
#include "Math.h"
#include "SDL.h"
#include <fstream>
#include <sstream>
#include "AssetLoader.h"
#include "Game.h"

bool InitializeRenderer(GameData *gd)
{
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  glEnable(GL_DEPTH_TEST);
  gd->context = SDL_GL_CreateContext(gd->window);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK)
  {
    SDL_Log("Failed to initialize GLEW.");
    return false;
  }
  return true;
}

uint32_t CreateSpriteVerts()
{
  // clang-format off
    std::vector<float> vertices{
       -0.5f,   0.5f,  0.f,  0.f,  0.f,
        0.5f,   0.5f,  0.f,  1.f,  0.f,
        0.5f,  -0.5f,  0.f,  1.f,  1.f,
       -0.5f,  -0.5f,  0.f,  0.f,  1.f
    };

    std::vector<unsigned int> indices{
      0,1,2,
      2,3,0
    };

    return LoadMesh(vertices, indices);
    //clang-format on
}

uint32_t LoadMesh(std::vector<float> vertices, std::vector<unsigned int> indices) 
{
  unsigned int vertexArray;
  unsigned int vertexBuffer;
  unsigned int indexBuffer;
  glGenVertexArrays(1, &vertexArray);
  glBindVertexArray(vertexArray);

  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5,
                        reinterpret_cast<void *>(sizeof(float) * 3));
  return vertexArray;
}


uint32_t LoadSkinnedMesh(const std::vector<SkinnedVertex>& vertices, const std::vector<unsigned int>& indices)
{
    unsigned int vertexArray, vertexBuffer, indexBuffer;

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinnedVertex), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    size_t stride = sizeof(SkinnedVertex);

    // Position (layout = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, m_Position));

    // Color (layout = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, m_Color));

    // Normal (layout = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, m_Normal));

    // UV (layout = 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, m_UV));

    // Tangent (layout = 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, m_Tangent));

    // Joint IDs (layout = 5)
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, stride, (void*)offsetof(SkinnedVertex, m_JointIds));

    // Weights (layout = 6)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, m_Weights));

    glBindVertexArray(0);

    return vertexArray;
}

uint32_t UploadMeshToGPU(const std::vector<uint32_t>& indices,const std::vector<float>& vertices, size_t stride) {
    uint32_t vertexArray;
    uint32_t vertexBuffer;
    uint32_t indexBuffer;
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
        vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),
        indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,  stride, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,  stride,
        reinterpret_cast<void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,  stride,
        reinterpret_cast<void*>(sizeof(float) * 6));
    return vertexArray;
}

Texture LoadTexture(const std::string &fileName) {
  Texture t;  
  ImageFile image(fileName);

  glGenTextures(1, &t.textureID);
  glBindTexture(GL_TEXTURE_2D, t.textureID);

  glTexImage2D(GL_TEXTURE_2D, 0, image.channels, image.width, image.height, 0, image.channels,
               GL_UNSIGNED_BYTE, image.data);
 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  t.texWidth = image.width;
  t.texHeight = image.height;
  t.scale = 1;
  return t;
}

void SetTextureActive(unsigned int textureID) {
  glBindTexture(GL_TEXTURE_2D, textureID);
}

void SetShaderActive(unsigned int shaderProgram) {
  glUseProgram(shaderProgram);
}

void SetVerticesActive(unsigned int vertexID) { 
    glBindVertexArray(vertexID); }

void SetMeshActive(unsigned int vertexID) {
    glBindVertexArray(vertexID);
}

unsigned int GetVertexArray() { 
    return 0; 
}

void SetMatrixUniform(
    unsigned int shaderProgram, 
    const char *uniformName,
    const Matrix4 *matrix) 
{
  GLuint loc = glGetUniformLocation(shaderProgram, uniformName);
  glUniformMatrix4fv(loc, 1, GL_TRUE, matrix->GetAsFloatPtr());
}

void SetVectorUniform(unsigned int shaderProgram, const char* name, const Vector3& vector)
{
    GLuint loc = glGetUniformLocation(shaderProgram, name);
    glUniform3fv(loc, 1, vector.GetAsFloatPtr());
}

bool IsCompiled(GLuint shader) {
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (status != GL_TRUE) {
    char buffer[512];
    memset(buffer, 0, 512);
    glGetShaderInfoLog(shader, 511, nullptr, buffer);
    SDL_Log("GLSL Compile Failed:\n%s", buffer);
    return false;
  }

  return true;
}

bool CompileShader(const std::string &fileName, GLenum shaderType,
                   GLuint &outShader) {
  // Open file
  std::ifstream shaderFile(fileName);
  if (shaderFile.is_open()) {
    // Read all the text into a string
    std::stringstream sstream;
    sstream << shaderFile.rdbuf();
    std::string contents = sstream.str();
    const char *contentsChar = contents.c_str();

    // Create a shader of the specified type
    outShader = glCreateShader(shaderType);
    // Set the source characters and try to compile
    glShaderSource(outShader, 1, &(contentsChar), nullptr);
    glCompileShader(outShader);

    if (!IsCompiled(outShader)) {
      SDL_Log("Failed to compile shader %s", fileName.c_str());
      return false;
    }
  } else {
    SDL_Log("Shader file not found: %s", fileName.c_str());
    return false;
  }

  return true;
}

bool IsValidProgram(GLuint mShaderProgram) {
  GLint status;
  // Query the link status
  glGetProgramiv(mShaderProgram, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    char buffer[512];
    memset(buffer, 0, 512);
    glGetProgramInfoLog(mShaderProgram, 511, nullptr, buffer);
    SDL_Log("GLSL Link Status:\n%s", buffer);
    return false;
  }

  return true;
}

Shader LoadShader(const std::string &vertexShader,
                  const std::string &fragmentShader) {
  GLuint mVertexShader;
  GLuint mFragShader;
  if (!CompileShader(vertexShader, GL_VERTEX_SHADER, mVertexShader) ||
      !CompileShader(fragmentShader, GL_FRAGMENT_SHADER, mFragShader)) {
    SDL_Log("Shader program failed to compile");
    return Shader{0};
  }

  // Now create a shader program that
  // links together the vertex/frag shaders
  GLuint mShaderProgram = glCreateProgram();
  glAttachShader(mShaderProgram, mVertexShader);
  glAttachShader(mShaderProgram, mFragShader);
  glLinkProgram(mShaderProgram);

  // Verify that the program linked successfully
  if (!IsValidProgram(mShaderProgram)) {
    SDL_Log("Shader program failed to link");
  }

  return Shader{mShaderProgram};
}

void BeginDrawOpaque()
{
    glClearColor(0.86f, 0.86f, 0.86f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void BeginDrawTransparent() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
}

void EndDraw(GameData* gd) {
    SDL_GL_SwapWindow(gd->window);
}
void DrawMesh(size_t numVertices) {
    glDrawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, 0);
}