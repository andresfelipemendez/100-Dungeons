#include "Systems.h"
#include "Components.h"
#include "Game.h"
#include "Math.h"
#include "Renderer.h"

void InputSystem(entt::registry *registry, const Uint8 *keyState) {
  auto input = registry->view<const Input, Move>();
  input.each([keyState](const Input &input, Move &move) {
    float forwardSpeed = 0.0f;
    if (keyState[input.forwardKey]) {
      forwardSpeed += input.maxFwdSpeed;
    }
    if (keyState[input.backKey]) {
      forwardSpeed -= input.maxFwdSpeed;
    }
    move.forwardSpeed = forwardSpeed;

    float angularSpeed = 0.0f;
    if (keyState[input.clockwiseKey]) {
      angularSpeed += input.maxAngSpeed;
    }
    if (keyState[input.counterClockwiseKey]) {
      angularSpeed -= input.maxAngSpeed;
    }
    move.angularSpeed = angularSpeed;
  });
}

void ShootingSystem(entt::registry *registry, const Uint8 *keyState) {
  /*auto shooter = registry->view<Shoot, const Transform>();
  shooter.each([registry, keyState](Shoot &shoot, const Transform &pos) {
    if (keyState[shoot.shootKey] && shoot.shootCooldown < 0.0f) {
      auto laser = registry->create();
      registry->emplace<Laser>(laser, 1.0f, 11.0f);
      registry->emplace<Transform>(laser, pos.x, pos.y, pos.rot);
      registry->emplace<Move>(laser, 0.0f, 800.0f);
      Texture tex = GetTexture("Assets/Laser.png");
      registry->emplace<Texture>(laser, tex);
      registry->emplace<Shader>(
          laser, GetShader("Assets/Sprite.vert", "Assets/Sprite.frag"));
      shoot.shootCooldown = 0.5f;
    }
  });*/
}

void CollisionSystem(entt::registry *registry, float deltaTime) {
  /*registry->view<Shoot>().each(
      [deltaTime](Shoot &shoot) { shoot.shootCooldown -= deltaTime; });

  auto laserBeams = registry->view<Laser>();
  laserBeams.each([registry, deltaTime](const auto entity, Laser &laser) {
    laser.life -= deltaTime;
    if (laser.life < 0.0f) {
      registry->emplace<DestroyTag>(entity);
    }
  });

  auto laserBeamsCollision = registry->view<const Laser, const Transform>();
  auto asteroidCollision = registry->view<const Circle, const Transform>();

  for (auto [laserEntity, laser, laserPos] : laserBeamsCollision.each()) {
    for (auto [astEntity, circle, astPos] : asteroidCollision.each()) {
      Vector2 diff =
          Vector2(laserPos.x, laserPos.y) - Vector2(astPos.x, astPos.y);
      float distSq = diff.LengthSq();

      float radiiSq = laser.radius + circle.radius;
      radiiSq *= radiiSq;
      if (distSq <= radiiSq) {
        registry->emplace_or_replace<DestroyTag>(laserEntity);
        registry->emplace_or_replace<DestroyTag>(astEntity);
      }
    }
  }

  auto destroyEntities = registry->view<const DestroyTag>();
  registry->destroy(destroyEntities.begin(), destroyEntities.end());*/
}

void MovementSystem(entt::registry *registry, float deltaTime) {
  registry->view<const Move, Translation, Rotation>().each(
      [deltaTime](const Move &move, Translation&_pos, Rotation& rot) {
        if (!Math::NearZero(move.angularSpeed)) {
            float angle = move.angularSpeed * deltaTime;
            Quaternion inc(Vector3::UnitZ, angle);
            rot.rotation = Quaternion::Concatenate(rot.rotation, inc);
        }

        if (!Math::NearZero(move.forwardSpeed)) {
          Vector3 pos = _pos.position;
          Vector3 forward = Vector3::Transform(Vector3::UnitX, rot.rotation);
          pos += forward * move.forwardSpeed * deltaTime;
          _pos.position = pos;
        }
      });
}

void RenderSystem(GameData *gd, entt::registry *registry) {
  auto camView = registry->view<Camera, Translation, Rotation>();
  auto camIt = camView.begin();
  Camera& cam = camView.get<Camera>(*camIt);
  Translation& campos = camView.get<Translation>(*camIt);
  Rotation& camrot = camView.get<Rotation>(*camIt);
  auto ambientLightColorView = registry->view<AmbientLightColor>();
  auto alcIt = ambientLightColorView.begin();
  AmbientLightColor& alc = ambientLightColorView.get<AmbientLightColor>(*alcIt);

  auto directionalLightView = registry->view<DirectionalLight>();
  auto dlIt = directionalLightView.begin();
  DirectionalLight& dl = directionalLightView.get<DirectionalLight>(*dlIt);


  Vector3 forward = Vector3::Transform(Vector3::UnitX, camrot.rotation);
  Matrix4 camview = Matrix4::CreateLookAt(
      campos.position, // camera position
      campos.position + forward * 100.0f, // targe position
      Vector3::UnitZ // up
  );

  Matrix4 viewProj = camview * cam.projectionMatrix;

  Matrix4 invView = viewProj;
  invView.Invert();



  BeginDrawOpaque();
  auto spriteView = registry->view<
      const Shader, 
      const Mesh, 
      const Texture, 
      const Translation, 
      const Rotation>();
  spriteView.each([&viewProj,&alc, &invView, &dl]
    (
        const Shader& shader, 
        const Mesh& mesh, 
        const Texture& texture, 
        const Translation& transform, 
        const Rotation& rotation
    )
    {
        Matrix4 worldTransform = Matrix4::CreateScale(1);
        worldTransform *= Matrix4::CreateFromQuaternion(rotation.rotation);
        worldTransform *= Matrix4::CreateTranslation(transform.position);
        SetShaderActive(shader.programID);
        SetTextureActive(texture.textureID);
        SetMeshActive(mesh.arrayID);
        
        SetVectorUniform(shader.programID, "uCameraPos", invView.GetTranslation());
        SetVectorUniform(shader.programID, "uAmbientLight", alc.color);
        SetVectorUniform(shader.programID, "uDirLight.mDirection", dl.direction);
        SetVectorUniform(shader.programID, "uDirLight.mDiffuseColor", dl.diffuseColor);
        SetVectorUniform(shader.programID, "uDirLight.mSpecColor", dl.specColor);
        SetMatrixUniform(shader.programID, "uViewProj", &viewProj);
        SetMatrixUniform(shader.programID, "uWorldTransform", &worldTransform);
        DrawMesh(mesh.numVerts);
    }
  );

  auto multimeshview = registry->view<
      const Shader,
      const MultiMesh,
      const Texture,
      const Translation,
      const Rotation>();
  multimeshview.each([&viewProj, &alc, &invView, &dl]
  (
      const Shader& shader,
      const MultiMesh& meshes,
      const Texture& texture,
      const Translation& transform,
      const Rotation& rotation
      )
      {
          Matrix4 worldTransform = Matrix4::CreateScale(1);
          worldTransform *= Matrix4::CreateFromQuaternion(rotation.rotation);
          worldTransform *= Matrix4::CreateTranslation(transform.position);
          SetShaderActive(shader.programID);
          SetTextureActive(texture.textureID);
          for (const auto& mesh : meshes.meshes)
          {
              SetMeshActive(mesh.arrayID);
              SetVectorUniform(shader.programID, "uCameraPos", invView.GetTranslation());
              SetVectorUniform(shader.programID, "uAmbientLight", alc.color);
              SetVectorUniform(shader.programID, "uDirLight.mDirection", dl.direction);
              SetVectorUniform(shader.programID, "uDirLight.mDiffuseColor", dl.diffuseColor);
              SetVectorUniform(shader.programID, "uDirLight.mSpecColor", dl.specColor);
              SetMatrixUniform(shader.programID, "uViewProj", &viewProj);
              SetMatrixUniform(shader.programID, "uWorldTransform", &worldTransform);
              DrawMesh(mesh.numVerts);
          }
      }
  );


  BeginDrawTransparent();
  auto view = registry->view<
      const Shader,
      const Mesh,
      const Sprite,
      const Translation,
      const Rotation>();
  view.each([&viewProj]
  (
      const Shader& shader,
      const Mesh& mesh,
      const Sprite& sprite,
      const Translation& transform,
      const Rotation& rotation
      )
      {
          Matrix4 viewProj = Matrix4::CreateSimpleViewProj(1024.f, 768.f);
          SetTextureActive(sprite.textureID);
          SetShaderActive(shader.programID);
          
          Matrix4 scaleMat = Matrix4::CreateScale
          (
              static_cast<float>(sprite.texWidth),
              static_cast<float>(sprite.texHeight),
              1.0f
          );

          Matrix4 worldTransform = Matrix4::CreateScale(1);
          Matrix4 world = scaleMat * worldTransform;
          SetMatrixUniform(shader.programID, "uWorldTransform", &world);
          SetMatrixUniform(shader.programID, "uViewProj", &viewProj);
          SetMeshActive(mesh.arrayID);
          DrawMesh(mesh.numVerts);
      }
  );

  EndDraw(gd);
}
