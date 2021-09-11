#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <unistd.h>
#include <unordered_set>
#include <string>
#include <vector>

unsigned in_row = 0;

void row_check()
{
    in_row++;
    // TODO: Make this dynamic
    if(in_row >= 8)
    {
        std::cout << std::endl;
        in_row = 0;
    }
}

void print_sgr_str(std::string const& value_str)
{
    std::cout << std::setw(4) << value_str << ": \e[" << value_str << "m" << "test" << "\e[m";
    row_check();
}

void print_sgr(int value)
{
    print_sgr_str(std::to_string(value));
}

// r,g,b in range 0-255
struct ColorRGB
{
    float r = 0, g = 0, b = 0;
    ColorRGB() = default;
    ColorRGB(float r_, float g_, float b_)
    : r(r_), g(g_), b(b_) {}
};

// h in range 0-360, s,v in range 0-1
struct ColorHSV
{
    float h, s, v;
};

// https://www.rapidtables.com/convert/color/hsv-to-rgb.html
ColorRGB hsv_to_rgb(ColorHSV const& in)
{
    assert(in.h >= 0 && in.h <= 360);
    assert(in.s >= 0 && in.s <= 1);
    assert(in.v >= 0 && in.v <= 1);
    float C = in.v * in.s;
    float Hp = static_cast<int>(in.h) / 60.f;
    float X = C * (1 - std::abs(std::fmod(Hp, 2.f) - 1));

    ColorRGB rgb_p;
    if(Hp >= 0 && Hp <= 1)
        rgb_p = ColorRGB(C, X, 0);
    else if(Hp > 1 && Hp <= 2)
        rgb_p = ColorRGB(X, C, 0);
    else if(Hp > 2 && Hp <= 3)
        rgb_p = ColorRGB(0, C, X);
    else if(Hp > 3 && Hp <= 4)
        rgb_p = ColorRGB(0, X, C);
    else if(Hp > 4 && Hp <= 5)
        rgb_p = ColorRGB(X, 0, C);
    else if(Hp > 5 && Hp <= 6)
        rgb_p = ColorRGB(C, 0, X);
        
    float m = in.v - C;
    return ColorRGB((rgb_p.r + m) * 255, (rgb_p.g + m) * 255, (rgb_p.b + m) * 255);
}

std::string sgr_rgb_from_color(ColorRGB const& in)
{
    return "2;" + std::to_string(static_cast<int>(in.r)) + ";" + std::to_string(static_cast<int>(in.g)) + ";" + std::to_string(static_cast<int>(in.b));
}

void wrap()
{
    std::cout << std::endl;
    in_row = 0;
}

void print_label(std::string const& value)
{
    std::cout << std::endl << std::endl << "\e[m" << value << std::endl << std::endl;
    in_row = 0;
}

class TestRunner
{
public:
    TestRunner(std::unordered_set<std::string> const& input_types)
    : m_types(resolve_types(input_types)) {}

    template<class Callback>
    void test(std::string const& label, std::string const& type, Callback&& callback) const
    {
        if(m_types.count(type))
        {
            print_label(label + " (" + type + ")");
            callback();
        }
    }

private:
    std::unordered_set<std::string> m_types;

    static std::unordered_set<std::string> resolve_types(std::unordered_set<std::string> const& type_names);
};

#define CONTINUE_IF_TRUE(...) if(__VA_ARGS__) continue;

std::unordered_set<std::string> TestRunner::resolve_types(std::unordered_set<std::string> const& type_names)
{
    std::unordered_set<std::string> output;

    auto expand_types = [&output](std::string const& type, std::string const& input, std::unordered_set<std::string> const& expand) {
        if(type == input)
        {
            auto sub_types = resolve_types(expand);
            output.insert(sub_types.begin(), sub_types.end());
            return true;
        }
        return false;
    };

    for(auto& type: type_names)
    {
        CONTINUE_IF_TRUE(expand_types(type, "sgr_standard", {"sgr_fg", "sgr_bg"}));
        CONTINUE_IF_TRUE(expand_types(type, "sgr_color", {"sgr_standard", "sgr_256", "sgr_rgb"}));
        CONTINUE_IF_TRUE(expand_types(type, "sgr", {"sgr_basic", "sgr_color"}));
        CONTINUE_IF_TRUE(expand_types(type, "all", {"sgr"}));
        output.insert(type);
    }
    return output;
}

int main(int argc, char** argv)
{
    if(!isatty(1))
    {
        std::cerr << "This program must run in a TTY." << std::endl;
        return 0;
    }

    std::string tests_string;
    if(argc == 1)
        tests_string = "all";
    else if(argc == 2)
    {
        tests_string = argv[1];
        if(tests_string == "--help")
        {
            std::cerr << "Usage: " << argv[0] << " [<tests>|--help]" << std::endl;
            std::cerr << "------------------------------------------" << std::endl;
            std::cerr << "type: Tests to run (comma-separated):" << std::endl;
            std::cerr << "  - sgr_basic: SGR basic" << std::endl;
            std::cerr << "  - sgr_fg: SGR foreground" << std::endl;
            std::cerr << "  - sgr_bg: SGR foreground" << std::endl;
            std::cerr << "  - sgr_256: SGR 256-color" << std::endl;
            std::cerr << "  - sgr_rgb: SGR RGB" << std::endl;
            std::cerr << "------------------------------------------" << std::endl;
            std::cerr << "  - sgr_standard = sgr_fg, sgr_bg" << std::endl;
            std::cerr << "  - sgr_color = sgr_standard, sgr_256, sgr_rgb" << std::endl;
            std::cerr << "  - sgr = sgr_basic, sgr_color" << std::endl;
            std::cerr << "  - all = sgr" << std::endl;
            std::cerr << "------------------------------------------" << std::endl;
            return 1;
        }
    }

    auto runner = TestRunner([&tests_string]() {
        std::unordered_set<std::string> out;
        std::istringstream iss(tests_string);
        while(!iss.eof())
        {
            std::string test;
            while(true)
            {
                char c = iss.peek();
                if(iss.peek() == ',' || iss.eof())
                    break;
                test += c;
                iss.ignore(1);
            }
            out.insert(test);
        }
        return out;
    }());

    runner.test("SGR: Basic", "sgr_basic", []() {
        for(int i = 1; i <= 29; i++)
            print_sgr(i);
        wrap();
        for(int i = 50; i <= 75; i++)
            print_sgr(i);
    });

    runner.test("SGR: Foreground", "sgr_fg", []() {
        for(int i = 30; i <= 37; i++)
            print_sgr(i);
        print_sgr(39);
        wrap();
        for(int i = 90; i <= 97; i++)
            print_sgr(i);
        print_sgr(99);
    });

    runner.test("SGR: Background", "sgr_bg", []() {
        for(int i = 40; i <= 47; i++)
            print_sgr(i);
        print_sgr(49);
        wrap();
        for(int i = 100; i <= 107; i++)
            print_sgr(i);
        print_sgr(109);
    });

    runner.test("SGR: 256-Color", "sgr_256", []() {
        std::cout << "Std  ";
        for(size_t s = 0; s < 16; s++)
        {
            std::cout << "\e[48;5;" << s << "m  ";
        }
        std::cout << "\e[m" << std::endl;
        for(size_t s = 0; s < 6; s++)
        {
            std::cout << std::left << std::setw(5) << s * 36;
            for(size_t t = 0; t < 36; t++)
            {
                std::cout << "\e[48;5;" << 16 + s * 36 + t << "m  ";
            }
            std::cout << "\e[m" << std::endl;
        }
        std::cout << "Gray ";
        for(size_t s = 232; s < 256; s++)
        {
            std::cout << "\e[48;5;" << s << "m  ";
        }
        std::cout << "\e[m";
    });

    runner.test("SGR: RGB Color", "sgr_rgb", []() {
        for(float h = 0; h < 16; h++)
        {
            float h1 = (h * 2) * 360 / 32;
            float h2 = (h * 2 + 1) * 360 / 32;
            std::cout << " h=" << std::setw(3) << static_cast<int>(h1) << " | ";
            for(float v = 0; v < 32; v++)
            {
                std::cout << "\e[38;" << sgr_rgb_from_color(hsv_to_rgb(ColorHSV{h1, v / 32.f, 1})) << "m"
                        << "\e[48;" << sgr_rgb_from_color(hsv_to_rgb(ColorHSV{h2, v / 32.f, 1})) << "m"
                        << "▀";
            }
            std::cout << "\e[0m" << std::endl;
        }
    });

    std::cout << std::endl;
    return 0;
}
