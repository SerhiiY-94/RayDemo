#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <Ray/RendererBase.h>
#include <Ray/Types.h>
#include <Sys/Json.h>

std::shared_ptr<Ray::SceneBase> LoadScene(Ray::RendererBase *r, const JsObject &js_scene);

std::tuple<std::vector<float>, std::vector<unsigned>, std::vector<unsigned>> LoadOBJ(const std::string &file_name);
std::tuple<std::vector<float>, std::vector<unsigned>, std::vector<unsigned>> LoadBIN(const std::string &file_name);

std::vector<Ray::pixel_color8_t> LoadTGA(const std::string &name, int &w, int &h);
std::vector<Ray::pixel_color8_t> LoadHDR(const std::string &name, int &w, int &h);
std::vector<Ray::pixel_color8_t> Load_stb_image(const std::string &name, int &w, int &h);