#include "GSCurveTest.h"

#include <fstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#endif

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <sys/Time_.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../comp/Drawable.h"
#include "../comp/Transform.h"
#include "../renderer/Renderer.h"

namespace GSPackTestInternal {
const float CAM_CENTER[] = { 0, 0, -1 },
                           CAM_TARGET[] = { 0, 0, 0 },
                                   CAM_UP[] = { 0, 1, 0 };

struct Img {
    int id;
    int w, h;

    Img(int _id, int _w, int _h) : id(_id), w(_w), h(_h) {}
};

struct Rect {
    int x, y, w, h;

    Rect(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h) {}
};

struct Node {
    int parent = -1;
    int child[2] = { -1, -1 };
    Rect rc;
    int num_free = 0;
    int tex_id = -1;

    Node() : rc(0, 0, 0, 0) {}

    static int Insert(std::vector<Node> &nodes, int i, const Img &img) {
        int ch0 = nodes[i].child[0], ch1 = nodes[i].child[1];

        if (nodes[i].num_free < (img.w * img.h) || nodes[i].tex_id != -1 || img.w > nodes[i].rc.w || img.h > nodes[i].rc.h) {
            return -1;
        }

        if (ch0 != -1) {
            int new_node = Insert(nodes, ch0, img);
            if (new_node != -1) return new_node;

            return Insert(nodes, ch1, img);
        } else {
            if (img.w == nodes[i].rc.w && img.h == nodes[i].rc.h) {
                nodes[i].num_free = 0;
                nodes[i].tex_id = img.id;
                return i;
            }

            nodes[i].child[0] = ch0 = (int)nodes.size();
            nodes.emplace_back();
            nodes[i].child[1] = ch1 = (int)nodes.size();
            nodes.emplace_back();

            auto rc = nodes[i].rc;

            int dw = rc.w - img.w;
            int dh = rc.h - img.h;

            if (dw > dh) {
                nodes[ch0].rc = { rc.x, rc.y, img.w, rc.h };
                nodes[ch1].rc = { rc.x + img.w, rc.y, rc.w - img.w, rc.h };
            } else {
                nodes[ch0].rc = { rc.x, rc.y, rc.w, img.h };
                nodes[ch1].rc = { rc.x, rc.y + img.h, rc.w, rc.h - img.h };
            }

            nodes[ch0].num_free = nodes[ch0].rc.w * nodes[ch0].rc.h;
            nodes[ch1].num_free = nodes[i].num_free - nodes[ch0].num_free;
            nodes[ch0].parent = nodes[ch1].parent = i;

            return Insert(nodes, ch0, img);
        }
    }

    /*static void SafeErase(std::vector<Node> &nodes, int i) {
    	int last = (int)nodes.size() - 1;
    	if (last == i) {
    		nodes.erase(nodes.begin() + i);
    		return;
    	}

    	int par = nodes[last].parent;

    	bool b = nodes[i].parent == last;

    	if (nodes[par].child[0] == last) {
    		nodes[par].child[0] = i;
    	} else if (nodes[par].child[1] == last) {
    		nodes[par].child[1] = i;
    	}

    	nodes[i] = nodes[last];

    	int ch0 = nodes[i].child[0],
    		ch1 = nodes[i].child[1];

    	if (ch0 != -1 && !b) {
    		if (nodes[ch0].parent != last || nodes[ch1].parent != last) {
    			__debugbreak();
    		}
    		nodes[ch0].parent = i;
    		nodes[ch1].parent = i;
    	}

    	nodes.erase(nodes.begin() + last);
    }*/

    static void SafeErase(std::vector<Node> &nodes, int i, int *indices, int num) {
        int last = (int)nodes.size() - 1;

        if (last != i) {
            int ch0 = nodes[last].child[0],
                ch1 = nodes[last].child[1];

            if (ch0 != -1 && nodes[i].parent != last) {
                nodes[ch0].parent = nodes[ch1].parent = i;
            }

            int par = nodes[last].parent;

            if (nodes[par].child[0] == last) {
                nodes[par].child[0] = i;
            } else if (nodes[par].child[1] == last) {
                nodes[par].child[1] = i;
            }

            nodes[i] = nodes[last];
        }

        nodes.erase(nodes.begin() + last);

        for (int j = 0; j < num && indices; j++) {
            if (indices[j] == last) {
                indices[j] = i;
            }
        }
    }

    static bool FullCheck(const std::vector<Node> &nodes) {
        for (int i = 0; i < (int)nodes.size(); i++) {
            int par = nodes[i].parent;
            if (par != -1) {
                if (nodes[par].child[0] != i && nodes[par].child[1] != i) {
                    return false;
                }
            }

            int ch0 = nodes[i].child[0],
                ch1 = nodes[i].child[1];

            if (ch0 != -1) {
                if (nodes[ch0].parent != i || nodes[ch1].parent != i) {
                    return false;
                }
            }
        }
        return true;
    }
};

std::vector<Node> nodes;

const int RES = 2048;
}

GSPackTest::GSPackTest(GameBase *game) : game_(game),
    cam_(GSPackTestInternal::CAM_CENTER,
         GSPackTestInternal::CAM_TARGET,
         GSPackTestInternal::CAM_UP) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
    cam_.Orthographic(1, -1, -1, 1, 0.1f, 10.0f);

    num_free_pixels_ = GSPackTestInternal::RES * GSPackTestInternal::RES;
}

GSPackTest::~GSPackTest() {

}

void GSPackTest::Enter() {
#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);

    swSetFloat(SW_CURVE_TOLERANCE, 1.0f);
#endif

    for (int i = 0; i < 255; i++) {
        color_table_[i] = { (rand() % 255) / 255.0f, (rand() % 255) / 255.0f, (rand() % 255) / 255.0f };
    }

    using namespace GSPackTestInternal;

    nodes.emplace_back();
    nodes.back().rc = { 0, 0, 2048, 2048 };
    nodes.back().num_free = 2048 * 2048;

    int t1 = sys::GetTicks();

    for (int i = 0; i < 20000; i++) {
        int img_w = 1 + rand() % (20-1), img_h = 7 + rand() % (20-7);

        int res = Node::Insert(nodes, 0, { num_textures_, img_w, img_h });
        if (res != -1) {
            num_textures_++;
            num_free_pixels_ -= img_w * img_h;

            //LOGI("adding img (%ix%i), num_free = %i (%f %%)", img_w, img_h, num_free_pixels_, 100.0f * num_free_pixels_ / (RES * RES));

            /*if (!Node::FullCheck(nodes)) {
            	__debugbreak();
            }*/

            int par = nodes[res].parent;

            while (par != -1) {
                nodes[par].num_free -= img_w * img_h;

                par = nodes[par].parent;
            }
        } else {
            //LOGI("add failed");
        }
    }

    LOGI("time: %i", int(sys::GetTicks() - t1));
}

void GSPackTest::Exit() {

}

void GSPackTest::Draw(float dt_s) {
    using namespace GSPackTestInternal;
    using namespace math;

    renderer_->set_current_cam(&cam_);
    renderer_->ClearColorAndDepth(0, 0, 0, 1);

    const vec2 offset = { -10.0f, -10.0f }, scale = { 20.0f / RES, 20.0f / RES };

    for (const auto &n : nodes) {
        if (n.tex_id == -1) continue;

        vec2 p = { (float)n.rc.x, (float)n.rc.y };
        vec2 d = { (float)n.rc.w, (float)n.rc.h };

        p = offset + scale * p;
        d = scale * d;

        renderer_->DrawRect(p, d, color_table_[n.tex_id % 255]);
    }

    ctx_->ProcessTasks();
}

void GSPackTest::Update(int dt_ms) {
    using namespace GSPackTestInternal;

    if (should_add_) {
        time_acc_ms_ += dt_ms;

        while (time_acc_ms_ > 1) {
            int img_w = 1 + rand() % (63), img_h = 1 + rand() % (63);
            img_w *= 1 + rand() % 1;
            img_h *= 1 + rand() % 1;

            int res = Node::Insert(nodes, 0, { num_textures_, img_w, img_h });
            if (res != -1) {
                num_textures_++;
                num_free_pixels_ -= img_w * img_h;

                LOGI("adding img (%ix%i), num_free = %i (%f %%)", img_w, img_h, num_free_pixels_, 100.0f * num_free_pixels_/(RES * RES));

                if (!Node::FullCheck(nodes)) {
                    //__debugbreak();
                }
            } else {
                LOGI("add failed");
            }

            time_acc_ms_ -= 1;
        }
    }

    auto has_children = [](const Node &n) -> bool {
        return n.child[0] != -1 || n.child[1] != -1;
    };

    if (should_delete_ && !should_add_ && !nodes.empty()) {
        time_acc_ms_ += dt_ms;

        while (time_acc_ms_ > 1) {
            int i = rand() % nodes.size();

            int start_i = i;
            while (true) {
                i = (i + 1) % nodes.size();

                if (nodes[i].tex_id != -1) {
                    num_textures_--;
                    num_free_pixels_ += nodes[i].rc.w * nodes[i].rc.h;
                    nodes[i].num_free = nodes[i].rc.w * nodes[i].rc.h;
                    nodes[i].tex_id = -1;

                    int img_w = nodes[i].rc.w;
                    int img_h = nodes[i].rc.h;

                    LOGI("deleting img %i (%ix%i), num_free = %i (%f %%)", i, nodes[i].rc.w, nodes[i].rc.h, num_free_pixels_, 100.0f * num_free_pixels_/(RES * RES));

                    int par = nodes[i].parent;
                    while (par != -1) {
                        nodes[par].num_free += img_w * img_h;
                        par = nodes[par].parent;
                    }

                    par = nodes[i].parent;

                    //assert(ch0 == i || ch1 == i);

                    while (par != -1) {
                        int ch0 = nodes[par].child[0],
                            ch1 = nodes[par].child[1];

                        if (!has_children(nodes[ch0]) && nodes[ch0].tex_id == -1 &&
                                !has_children(nodes[ch1]) && nodes[ch1].tex_id == -1) {

                            LOGI("deleting childs of %i: %i %i (%i)", par, ch0, ch1, int(nodes.size()));

                            if (!Node::FullCheck(nodes)) {
                                //__debugbreak();
                            }

                            Node::SafeErase(nodes, ch0, &par, 1);
                            ch1 = nodes[par].child[1];
                            Node::SafeErase(nodes, ch1, &par, 1);

                            nodes[par].child[0] = nodes[par].child[1] = -1;

                            if (!Node::FullCheck(nodes)) {
                                //__debugbreak();
                            }

                            par = nodes[par].parent;
                        } else {
                            break;
                        }
                    }
                    break;
                }

                if (i == start_i) break;
            }

            time_acc_ms_ -= 1;
        }
    }
}

void GSColorsTest::HandleInput(InputManager::Event evt) {
    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN: {
        int w = game_->width, h = game_->height;

        grabbed_ = true;
    }
    break;
    case InputManager::RAW_INPUT_P1_UP: {

    } break;
    case InputManager::RAW_INPUT_P1_MOVE: {
        int w = game_->width, h = game_->height;

    }
    break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.raw_key == 'a') {

        } else if (evt.raw_key == 'd') {

        }
    } break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.raw_key == 'a') {

        } else if (evt.raw_key == 'd') {

        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {

        }
    } break;
    case InputManager::RAW_INPUT_RESIZE:
        //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
        break;
    default:
        break;
    }
}
