#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <ray/RendererBase.h>
#include <ray/Types.h>
#include <sys/Json.h>

std::shared_ptr<ray::SceneBase> LoadScene(ray::RendererBase *r, const JsObject &js_scene);

std::tuple<std::vector<float>, std::vector<unsigned>, std::vector<unsigned>> LoadOBJ(const std::string &file_name);

std::vector<ray::pixel_color8_t> LoadTGA(const std::string &name, int &w, int &h);