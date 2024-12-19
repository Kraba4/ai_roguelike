#include <raylib.h>
#include "shootEmUp.h"
#include "ecsTypes.h"
#include "rlikeObjects.h"
#include "steering.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"
#include "pathfinder.h"
#include "hierarchicalPathfinder.h"
#include <iostream>

constexpr float tile_size = 64.f;

static void register_roguelike_systems(flecs::world &ecs)
{

  ecs.system<Velocity, const MoveSpeed, const IsPlayer>()
    .each([&](Velocity &vel, const MoveSpeed &ms, const IsPlayer)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      vel.x = ((left ? -1.f : 0.f) + (right ? 1.f : 0.f));
      vel.y = ((up ? -1.f : 0.f) + (down ? 1.f : 0.f));
      vel = Velocity{normalize(vel) * ms.speed};
    });

  ecs.system<Position, const Velocity>()
    .each([&](Position &pos, const Velocity &vel)
    {
      pos += vel * ecs.delta_time();
    });

  ecs.system<const Position, const Color>()
    .with<TextureSource>(flecs::Wildcard)
    .with<BackgroundTile>()
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x), float(pos.y), tile_size, tile_size}, color);
    });
  ecs.system<const Position, const Color>()
    .with<TextureSource>(flecs::Wildcard)
    .without<BackgroundTile>()
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x), float(pos.y), tile_size, tile_size}, color);
    });

  ecs.system<Texture2D>()
    .each([&](Texture2D &tex)
    {
      SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    });

  ecs.system<MonsterSpawner>()
    .each([&](MonsterSpawner &ms)
    {
      auto playerPosQuery = ecs.query<const Position, const IsPlayer>();
      playerPosQuery.each([&](const Position &pp, const IsPlayer &)
      {
        ms.timeToSpawn -= ecs.delta_time();
        while (ms.timeToSpawn < 0.f)
        {
          steer::Type st = steer::Type(GetRandomValue(0, steer::Type::Num - 1));
          const Color colors[steer::Type::Num] = {WHITE, RED, BLUE, GREEN};
          const float distances[steer::Type::Num] = {800.f, 800.f, 300.f, 300.f};
          const float dist = distances[st];
          constexpr int angRandMax = 1 << 16;
          const float angle = float(GetRandomValue(0, angRandMax)) / float(angRandMax) * PI * 2.f;
          Color col = colors[st];
          steer::create_steer_beh(create_monster(ecs,
              {pp.x + cosf(angle) * dist, pp.y + sinf(angle) * dist}, col, "minotaur_tex"), st);
          ms.timeToSpawn += ms.timeBetweenSpawns;
        }
      });
    });

  ecs.system<const Camera2D, HierarchicalPathFinder>()
  .each([&](const Camera2D& camera, HierarchicalPathFinder& h_pathfinder) {

      Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), camera);
      IVec2 p{int(mousePosition.x), int(mousePosition.y)};
      IVec2 from = h_pathfinder.getStart();
      IVec2 to = h_pathfinder.getEnd();
      IVec2 ceil_mouse_pos = IVec2 {(int) (p.x / tile_size), int(p.y / tile_size)};
      if (IsMouseButtonPressed(0))
      {
        from = ceil_mouse_pos;
      }
      else if (IsMouseButtonPressed(1))
      {
        to = ceil_mouse_pos;
      }
      auto dungeonQuery = ecs.query<DungeonPortals, DungeonData>();
      dungeonQuery.each([&](DungeonPortals &dp, DungeonData &dd) {
        h_pathfinder.find_path(dp, dd, from, to);
        std::vector<IVec2> detailed_path = h_pathfinder.get_detailed_path(ceil_mouse_pos, dp, dd);
        size_t from_i = 0;
        size_t to_i = 0;
        for (int i = 1; i < detailed_path.size(); ++i) {
          from_i = to_i;
          to_i = i;
          const IVec2& from = detailed_path[from_i];
          const IVec2& to   = detailed_path[to_i];
          Vector2 fromCenter{(from.x + from.x + 1) * tile_size * 0.5f,
                            (from.y + from.y + 1) * tile_size * 0.5f};
          Vector2 toCenter{(to.x + to.x + 1) * tile_size * 0.5f,
                          (to.y + to.y + 1) * tile_size * 0.5f};
          DrawLineEx(fromCenter, toCenter, 3.f, BLUE);
        }
      });

  });

  ecs.system<const DungeonPortals>()
  .each([&](const DungeonPortals &dp)
  {
    auto dungeonQuery = ecs.query<const HierarchicalPathFinder>();
      dungeonQuery.each([&](const HierarchicalPathFinder &h_pathfinder) {
      const std::vector<int> &path = h_pathfinder.get_path();
      size_t from_i = 0;
      size_t to_i = 0;

      for (int i = 1; i < path.size(); ++i) {
        from_i = to_i;
        to_i = i;
        const PathPortal& from = dp.portals[path[from_i]];
        const PathPortal& to   = dp.portals[path[to_i]];
        Vector2 fromCenter{(from.startX + from.endX + 1) * tile_size * 0.5f,
                          (from.startY + from.endY + 1) * tile_size * 0.5f};
        Vector2 toCenter{(to.startX + to.endX + 1) * tile_size * 0.5f,
                        (to.startY + to.endY + 1) * tile_size * 0.5f};
        DrawLineEx(fromCenter, toCenter, 8.f, BLUE);
    }
  });
  });

  ecs.system<const DungeonPortals, const DungeonData>()
    .each([&](const DungeonPortals &dp, const DungeonData &dd)
    {
      size_t w = dd.width;
      size_t ts = dp.tileSplit;
      for (size_t y = 0; y < dd.height / ts; ++y)
        DrawLineEx(Vector2{0.f, y * ts * tile_size},
                   Vector2{dd.width * tile_size, y * ts * tile_size}, 1.f, GetColor(0xff000080));
      for (size_t x = 0; x < dd.width / ts; ++x)
        DrawLineEx(Vector2{x * ts * tile_size, 0.f},
                   Vector2{x * ts * tile_size, dd.height * tile_size}, 1.f, GetColor(0xff000080));
      auto cameraQuery = ecs.query<const Camera2D>();
      cameraQuery.each([&](Camera2D cam)
      {
        Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), cam);
        size_t wd = w / ts;
        for (size_t y = 0; y < dd.height / ts; ++y)
        {
          if (mousePosition.y < y * ts * tile_size || mousePosition.y > (y + 1) * ts * tile_size)
            continue;
          for (size_t x = 0; x < dd.width / ts; ++x)
          {
            if (mousePosition.x < x * ts * tile_size || mousePosition.x > (x + 1) * ts * tile_size)
              continue;
            for (size_t idx : dp.tilePortalsIndices[y * wd + x])
            {
              const PathPortal &portal = dp.portals[idx];
              Rectangle rect{portal.startX * tile_size, portal.startY * tile_size,
                             (portal.endX - portal.startX + 1) * tile_size,
                             (portal.endY - portal.startY + 1) * tile_size};
              DrawRectangleLinesEx(rect, 5, BLACK);
            }
          }
        }
        for (const PathPortal &portal : dp.portals)
        {
          Rectangle rect{portal.startX * tile_size, portal.startY * tile_size,
                         (portal.endX - portal.startX + 1) * tile_size,
                         (portal.endY - portal.startY + 1) * tile_size};
          Vector2 fromCenter{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
          DrawRectangleLinesEx(rect, 1, WHITE);
          if (mousePosition.x < rect.x || mousePosition.x > rect.x + rect.width ||
              mousePosition.y < rect.y || mousePosition.y > rect.y + rect.height)
            continue;
          DrawRectangleLinesEx(rect, 4, WHITE);
          for (const PortalConnection &conn : portal.conns)
          {
            const PathPortal &endPortal = dp.portals[conn.connIdx];
            Vector2 toCenter{(endPortal.startX + endPortal.endX + 1) * tile_size * 0.5f,
                             (endPortal.startY + endPortal.endY + 1) * tile_size * 0.5f};
            DrawLineEx(fromCenter, toCenter, 1.f, WHITE);
            DrawText(TextFormat("%d", int(conn.score)),
                     (fromCenter.x + toCenter.x) * 0.5f,
                     (fromCenter.y + toCenter.y) * 0.5f,
                     16, WHITE);
          }
        }
      });
    });
  steer::register_systems(ecs);
}


void init_shoot_em_up(flecs::world &ecs)
{
  register_roguelike_systems(ecs);

  ecs.entity("swordsman_tex")
    .set(Texture2D{LoadTexture("assets/swordsman.png")});
  ecs.entity("minotaur_tex")
    .set(Texture2D{LoadTexture("assets/minotaur.png")});

  const Position walkableTile = dungeon::find_walkable_tile(ecs);
  create_player(ecs, walkableTile * tile_size, "swordsman_tex");
}

void init_dungeon(flecs::world &ecs, char *tiles, size_t w, size_t h)
{
  flecs::entity wallTex = ecs.entity("wall_tex")
    .set(Texture2D{LoadTexture("assets/wall.png")});
  flecs::entity floorTex = ecs.entity("floor_tex")
    .set(Texture2D{LoadTexture("assets/floor.png")});

  std::vector<char> dungeonData;
  dungeonData.resize(w * h);
  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x)
      dungeonData[y * w + x] = tiles[y * w + x];
  ecs.entity("dungeon")
    .set(DungeonData{dungeonData, w, h});

  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x)
    {
      char tile = tiles[y * w + x];
      flecs::entity tileEntity = ecs.entity()
        .add<BackgroundTile>()
        .set(Position{float(x) * tile_size, float(y) * tile_size})
        .set(Color{255, 255, 255, 255});
      if (tile == dungeon::wall)
        tileEntity.add<TextureSource>(wallTex);
      else if (tile == dungeon::floor)
        tileEntity.add<TextureSource>(floorTex);
    }
  ecs.entity("camera")
  .set(HierarchicalPathFinder{});
  prebuild_map(ecs);
}

void process_game(flecs::world &ecs)
{
}

