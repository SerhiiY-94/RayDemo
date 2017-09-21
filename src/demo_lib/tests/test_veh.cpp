#include "test_common.h"

#include "../veh/PlotGraph.h"

void test_veh() {
    using namespace std;

    {
        // Test plot
        vector<pair<int, float>> init_values = {{-8, 0},
            {-7, 1},
            {-6, 2},
            {-5, 3},
            {-4, 4},
            {-3, 3},
            {-2, 2},
            {-1, 1},
            {0, 0},
            {1, 1},
            {2, 2},
            {3, 2},
            {4, 1}
        };

        vector<pair<int, float>> check_values1 = {{-6, 2},
            {-3, 3},
            {0, 0}
        };

        vector<pair<float, float>> check_values2 = {{0.5f, 0.5f},
            {1, 1},
            {2, 2},
            {1.2f, 1.2f},
            {3.4f, 1.6f}
        };

        PlotGraph p(init_values[0].first, init_values.back().first, 1);
        assert(p.size() == init_values.size());

        for (auto i : init_values) {
            p.SetValue(i.first, i.second);
        }

        for (auto i : check_values1) {
            assert(p.GetValue(i.first) == i.second);
        }

        for (auto i : check_values2) {
            assert(p.GetValueLerp(i.first) == Approx(i.second));
        }
    }
}
