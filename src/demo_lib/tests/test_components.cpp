#include "test_common.h"

#include <sstream>

#include <sys/Json.h>

#include "../comp/LevelProperties.h"
#include "../comp/Transform.h"

void test_components() {
    {
        // Test Transform
        {
            // Move and rotate
            Transform tr({ -1, -2, -3 }, { 1, 2, 3 });

            assert(tr.id() == "Transform");
            assert(tr.bbox_min() == math::vec3(-1, -2, -3));
            assert(tr.bbox_max() == math::vec3(1, 2, 3));
            assert(tr.pos() == math::vec3(0, 0, 0));
            //assert(tr.rot() == glm::quat());
            assert(tr.angles() == math::vec3());

            tr.Move(1, 2, 3);

            assert(tr.orig_bbox_min() == math::vec3(-1, -2, -3));
            assert(tr.orig_bbox_max() == math::vec3(1, 2, 3));
            assert(tr.pos() == math::vec3(1, 2, 3));
            assert(tr.bbox_min() == math::vec3(0, 0, 0));
            assert(tr.bbox_max() == math::vec3(2, 4, 6));
            //assert(tr.rot() == glm::quat());
            assert(tr.angles() == math::vec3());

            tr.Rotate(45, 0, 0);

            assert(tr.angles().x() == Approx(45));
            assert(tr.angles().y() == Approx(0));
            assert(tr.angles().z() == Approx(0));
        }

        {
            // Read/Write
            Transform tr({ -1, -2, -3 }, { 1, 2, 3 });
            tr.Move(1, 2, 3);

            JsObject js_out;
            tr.Write(js_out);

            Transform tr2;
            assert(tr2.Read(js_out));
            assert(tr2.orig_bbox_min() == tr.orig_bbox_min());
            assert(tr2.orig_bbox_max() == tr.orig_bbox_max());
            assert(tr2.bbox_min().x() == Approx(0));
            assert(tr2.bbox_min().y() == Approx(0));
            assert(tr2.bbox_min().z() == Approx(0));
            assert(tr2.bbox_max().x() == Approx(2));
            assert(tr2.bbox_max().y() == Approx(4));
            assert(tr2.bbox_max().z() == Approx(6));
            assert(tr2.pos().x() == Approx(tr.pos().x()));
            assert(tr2.pos().y() == Approx(tr.pos().y()));
            assert(tr2.pos().z() == Approx(tr.pos().z()));
        }
    }

    {
        // Test LevelProperties
        {
            // Read
            LevelProperties lev_props;

            const std::string str =
                "{"
                "\t\"LevelProperties\" : {"
                "\t\t\"Cdrag\" : 0.145,"
                "\t\t\"Crr\" : 0.025"
                "\t}"
                "}";
            std::stringstream ss(str);
            JsObject js_obj;
            js_obj.Read(ss);

            lev_props.Read(js_obj);
            assert(lev_props.Cdrag() == Approx(0.145));
            assert(lev_props.Crr() == Approx(0.025));
        }
    }
}
