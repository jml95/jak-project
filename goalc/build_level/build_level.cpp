#include "common/custom_data/Tfrag3Data.h"
#include "common/log/log.h"
#include "common/util/FileUtil.h"
#include "common/util/compress.h"
#include "common/util/json_util.h"

#include "goalc/build_level/FileInfo.h"
#include "goalc/build_level/LevelFile.h"
#include "goalc/build_level/Tfrag.h"
#include "goalc/build_level/collide_bvh.h"
#include "goalc/build_level/collide_pack.h"
#include "goalc/build_level/gltf_mesh_extract.h"

#include "third-party/fmt/core.h"

void save_pc_data(const std::string& nickname, tfrag3::Level& data) {
  Serializer ser;
  data.serialize(ser);
  auto compressed =
      compression::compress_zstd(ser.get_save_result().first, ser.get_save_result().second);
  fmt::print("stats for {}\n", data.level_name);
  print_memory_usage(data, ser.get_save_result().second);
  fmt::print("compressed: {} -> {} ({:.2f}%)\n", ser.get_save_result().second, compressed.size(),
             100.f * compressed.size() / ser.get_save_result().second);
  file_util::write_binary_file(file_util::get_file_path({fmt::format("assets/{}.fr3", nickname)}),
                               compressed.data(), compressed.size());
}

std::vector<std::string> get_build_level_deps(const std::string& input_file) {
  auto level_json = parse_commented_json(
      file_util::read_text_file(file_util::get_file_path({input_file})), input_file);
  return {level_json.at("gltf_file").get<std::string>()};
}

bool run_build_level(const std::string& input_file, const std::string& output_file) {
  auto level_json = parse_commented_json(
      file_util::read_text_file(file_util::get_file_path({input_file})), input_file);
  LevelFile file;          // GOAL level file
  tfrag3::Level pc_level;  // PC level file
  TexturePool tex_pool;    // pc level texture pool

  // process input mesh from blender
  gltf_mesh_extract::Input mesh_extract_in;
  mesh_extract_in.filename =
      file_util::get_file_path({level_json.at("gltf_file").get<std::string>()});
  mesh_extract_in.tex_pool = &tex_pool;
  gltf_mesh_extract::Output mesh_extract_out;
  gltf_mesh_extract::extract(mesh_extract_in, mesh_extract_out);

  // add stuff to the GOAL level structure
  file.info = make_file_info_for_level(std::filesystem::path(input_file).filename().string());
  // all vis
  // drawable trees
  // pat
  // texture remap
  // texture ids
  // unk zero
  // name
  file.name = level_json.at("long_name").get<std::string>();
  // nick
  file.nickname = level_json.at("nickname").get<std::string>();
  // vis infos
  // actors
  // cameras
  // nodes
  // boxes
  // ambients
  // subdivs
  // adgifs
  // actor birht
  // split box

  // add stuff to the PC level structure
  pc_level.level_name = file.name;

  // TFRAG
  auto& tfrag_drawable_tree = file.drawable_trees.tfrags.emplace_back();
  tfrag_from_gltf(mesh_extract_out.tfrag, tfrag_drawable_tree,
                  pc_level.tfrag_trees[0].emplace_back());
  pc_level.textures = std::move(tex_pool.textures_by_idx);

  // COLLIDE
  if (mesh_extract_out.collide.faces.empty()) {
    lg::error("No collision geometry was found");
  } else {
    auto& collide_drawable_tree = file.drawable_trees.collides.emplace_back();
    collide_drawable_tree.bvh = collide::construct_collide_bvh(mesh_extract_out.collide.faces);
    collide_drawable_tree.packed_frags = pack_collide_frags(collide_drawable_tree.bvh.frags.frags);
  }

  // Save the GOAL level
  auto result = file.save_object_file();
  fmt::print("Level bsp file size {} bytes\n", result.size());
  auto save_path = file_util::get_file_path({output_file});
  file_util::create_dir_if_needed_for_file(save_path);
  fmt::print("Saving to {}\n", save_path);
  file_util::write_binary_file(save_path, result.data(), result.size());

  // Save the PC level
  save_pc_data(file.nickname, pc_level);

  return true;
}
