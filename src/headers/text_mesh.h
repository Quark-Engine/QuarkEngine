#include <string>
#include <vector>
#include "QuarkCore/QuarkCore.hpp"
using namespace qc;

void init_freetype();
void shutdown_freetype();

std::vector<std::pair<std::string, std::string>> get_system_fonts();
std::string get_default_font_path();
Model generate_text_mesh(const std::string& text, float size, float thickness, float letter_spacing, const std::string& font_path);