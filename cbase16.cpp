#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <span>
#include <unistd.h>
#include <vector>

#include <git2.h>
#include <yaml-cpp/yaml.h>

#if defined(__linux__)
#include <sys/ioctl.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

struct Template {
	std::string name;
	std::string data;
	std::string extension;
	std::string output;
};

struct Scheme {
	std::string slug;
	std::string name;
	std::string author;
	std::map<std::string, std::string> colors;
};

struct Terminal {
	int width;
	int height;
};

constexpr int HEX_MIN_LENGTH = 6;
constexpr int RGB_MIN_SIZE = 3;
constexpr int RGB_DEC = 255;

void clone(const std::filesystem::path &, const std::string &, const std::string &);
void update(const std::filesystem::path &, bool);
auto get_template(const std::filesystem::path &) -> std::vector<Template>;
auto get_scheme(const std::filesystem::path &) -> std::vector<Scheme>;
auto parse_template_dir(const std::filesystem::path &) -> std::vector<Template>;
auto parse_scheme_dir(const std::filesystem::path &) -> std::vector<Scheme>;
auto hex_to_rgb(const std::string &) -> std::vector<int>;
auto rgb_to_dec(const std::vector<int> &) -> std::vector<long double>;
void replace_all(std::string &, const std::string &, const std::string &);
void build(const std::filesystem::path &, const std::vector<std::string> &,
           const std::vector<std::string> &, const std::filesystem::path &,
           const std::filesystem::path &, bool);
auto get_terminal_size() -> Terminal;
void list_templates(const std::filesystem::path &, const bool &);
void list_schemes(const std::filesystem::path &, const bool &);
void list(const std::filesystem::path &, const bool &, const bool &, const bool &);

void
clone(const std::filesystem::path &opt_cache_dir, const std::string &dir, const std::string &source)
{
	if (std::filesystem::is_regular_file(source)) {
		YAML::Node file = YAML::LoadFile(source);

		std::vector<std::string> token_key;
		std::vector<std::string> token_value;

		for (YAML::const_iterator it = file.begin(); it != file.end(); ++it) {
			token_key.emplace_back(it->first.as<std::string>());
			token_value.emplace_back(it->second.as<std::string>());
		}

#pragma omp parallel for default(none) shared(token_key, token_value, opt_cache_dir, dir)
		for (int i = 0; i < token_key.size(); ++i) {
			git_repository *repo = nullptr;
			git_clone(&repo, token_value[i].c_str(),
			          (opt_cache_dir / dir / token_key[i]).c_str(), nullptr);
			git_repository_free(repo);
		}
	} else {
		std::cout << "error: cannot read " + source << std::endl;
		_exit(1);
	}
}

void
update(const std::filesystem::path &opt_cache_dir, bool legacy)
{
	std::ofstream file(opt_cache_dir / "sources.yaml");

	if (file.good()) {
		YAML::Emitter source;

		if (!legacy) {
			source << YAML::BeginMap;
			source << YAML::Key << "schemes";
			source << YAML::Value
			       << "https://github.com/base16-fork/base16-schemes-recipe.git";
			source << YAML::Key << "templates";
			source << YAML::Value
			       << "https://github.com/base16-fork/base16-templates-recipe.git";
			source << YAML::EndMap;
		} else {
			source << YAML::BeginMap;
			source << YAML::Key << "schemes";
			source << YAML::Value
			       << "https://github.com/chriskempson/base16-schemes-source.git";
			source << YAML::Key << "templates";
			source << YAML::Value
			       << "https://github.com/chriskempson/base16-templates-source.git";
			source << YAML::EndMap;
		}

		file << source.c_str();
		file.close();
	} else {
		std::cout << "error: fail to write sources.yaml to " << opt_cache_dir << std::endl;
		_exit(1);
	}

	git_libgit2_init();

	if (!legacy) {
		clone(opt_cache_dir, "", opt_cache_dir / "sources.yaml");
	} else {
		clone(opt_cache_dir, "sources", opt_cache_dir / "sources.yaml");
		clone(opt_cache_dir, "schemes",
		      opt_cache_dir / "sources" / "schemes" / "list.yaml");
		clone(opt_cache_dir, "templates",
		      opt_cache_dir / "sources" / "templates" / "list.yaml");
	}

	git_libgit2_shutdown();
}

auto
get_template(const std::filesystem::path &directory) -> std::vector<Template>
{
	std::vector<Template> templates;

	if (!std::filesystem::is_directory(directory)) {
		std::cout << "warning: " << directory
			  << " template directory is either empty or not found" << std::endl;
		return templates;
	}

	for (const std::filesystem::directory_entry &file :
	     std::filesystem::directory_iterator(directory)) {
		if (file.path().extension() == ".mustache") {
			Template templet;

			YAML::Node config = YAML::LoadFile(directory / "config.yaml");
			templet.name = directory.parent_path().stem().string();

			for (YAML::const_iterator it = config.begin(); it != config.end(); ++it) {
				YAML::Node node = config[it->first.as<std::string>()];
				if (it->first.as<std::string>() == file.path().stem().string()) {
					if (node["extension"].Type() != YAML::NodeType::Null)
						templet.extension =
							node["extension"].as<std::string>();

					templet.output = node["output"].as<std::string>();
				}
			}

			std::ifstream buffer(file.path().string(),
			                     std::ios::binary | std::ios::ate);

			if (buffer.good()) {
				buffer.seekg(0, std::ios::end);
				templet.data.resize(buffer.tellg());
				buffer.seekg(0, std::ios::beg);
				buffer.read(&templet.data[0], (long)templet.data.size());
				buffer.close();
			}

			templates.emplace_back(templet);
		}
	}

	return templates;
}

auto
parse_template_dir(const std::filesystem::path &directory) -> std::vector<Template>
{
	std::vector<Template> templates;

	for (const std::filesystem::directory_entry &entry :
	     std::filesystem::directory_iterator(directory)) {
		if (!std::filesystem::is_regular_file(entry.path() / "templates" / "config.yaml")) {
			continue;
		}

		std::vector<Template> parse_templates = get_template(entry.path() / "templates");

		templates.insert(templates.begin(), parse_templates.begin(), parse_templates.end());
	}

	return templates;
}

auto
get_scheme(const std::filesystem::path &directory) -> std::vector<Scheme>
{
	std::vector<Scheme> schemes;

	if (!std::filesystem::is_directory(directory)) {
		std::cout << "warning: " << directory
			  << " scheme directory is either empty or not found" << std::endl;
		return schemes;
	}

	for (const std::filesystem::directory_entry &file :
	     std::filesystem::directory_iterator(directory)) {
		if (file.is_regular_file() && file.path().extension() == ".yaml") {
			Scheme scheme;

			YAML::Node node = YAML::LoadFile(file.path().string());

			scheme.slug = file.path().stem().string();

			for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
				auto key = it->first.as<std::string>();
				auto value = it->second.as<std::string>();

				if (key == "scheme")
					scheme.name = value;
				else if (key == "author")
					scheme.author = value;
				else
					scheme.colors.insert({ key, value });
			}

			schemes.emplace_back(scheme);
		}
	}

	return schemes;
}

auto
parse_scheme_dir(const std::filesystem::path &directory) -> std::vector<Scheme>
{
	std::vector<Scheme> schemes;

	for (const std::filesystem::directory_entry &entry :
	     std::filesystem::directory_iterator(directory)) {
		std::vector<Scheme> parse_schemes = get_scheme(entry);

		schemes.insert(schemes.end(), parse_schemes.begin(), parse_schemes.end());
	}

	return schemes;
}

auto
hex_to_rgb(const std::string &hex) -> std::vector<int>
{
	std::vector<int> rgb(3);
	std::stringstream ss;
	std::string str;

	if (hex.size() != HEX_MIN_LENGTH)
		return rgb;

#pragma omp parallel for default(none) shared(hex, str, ss, rgb)
	for (int i = 0; i < 3; ++i) {
		str = hex.substr(i * 2, 2);
		ss << std::hex << str;
		ss >> rgb[i];
		ss.clear();
	}

	return rgb;
}

auto
rgb_to_dec(const std::vector<int> &rgb) -> std::vector<long double>
{
	std::vector<long double> dec(3);

	if (rgb.size() != RGB_MIN_SIZE)
		return dec;

#pragma omp parallel for default(none) shared(dec, rgb)
	for (int i = 0; i < 3; ++i)
		dec[i] = (long double)rgb[i] / RGB_DEC;

	return dec;
}

void
replace_all(std::string &str, const std::string &from, const std::string &to)
{
	if (from.empty())
		return;

	size_t start_pos = 0;

	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

void
build(const std::filesystem::path &opt_cache_dir, const std::vector<std::string> &opt_templates,
      const std::vector<std::string> &opt_schemes, const std::filesystem::path &opt_build_dir,
      const std::filesystem::path &opt_output, bool make)
{
	std::vector<Template> templates;
	std::vector<Scheme> schemes;

	if (make) {
		bool good = false;

		for (const std::filesystem::directory_entry &file :
		     std::filesystem::directory_iterator(opt_build_dir)) {
			if (file.is_regular_file() && file.path().extension() == ".yaml") {
				good = true;
				break;
			}
		}

		if (good) {
			std::vector<Scheme> parse_scheme = get_scheme(opt_build_dir);
			schemes.insert(schemes.begin(), parse_scheme.begin(), parse_scheme.end());
		} else {
			schemes = parse_scheme_dir(opt_cache_dir / "schemes");
		}
	} else {
		schemes = parse_scheme_dir(opt_cache_dir / "schemes");
	}

	if (make) {
		if (std::filesystem::is_directory(opt_build_dir / "templates")) {
			std::vector<Template> parse_templates =
				get_template(opt_build_dir / "templates");
			templates.insert(templates.begin(), parse_templates.begin(),
			                 parse_templates.end());
		} else {
			templates = parse_template_dir(opt_cache_dir / "templates");
		}
	} else {
		templates = parse_template_dir(opt_cache_dir / "templates");
	}

#pragma omp parallel for default(none) \
	shared(opt_templates, opt_schemes, templates, schemes, opt_build_dir, opt_output, make)
	for (const Scheme &s : schemes) {
		if (!opt_schemes.empty() &&
		    std::find(opt_schemes.begin(), opt_schemes.end(), s.slug) == opt_schemes.end())
			continue;

#pragma omp parallel for default(none) \
	shared(opt_templates, templates, s, opt_build_dir, opt_output, make)
		// NOLINTNEXTLINE (openmp-exception-escape)
		for (Template t : templates) {
			if (!opt_templates.empty() &&
			    std::find(opt_templates.begin(), opt_templates.end(), t.name) ==
			            opt_templates.end())
				continue;

			for (const auto &[base, color] : s.colors) {
				std::vector<std::string> hex = { color.substr(0, 2),
					                         color.substr(2, 2),
					                         color.substr(4, 2) };
				std::vector<int> rgb = hex_to_rgb(color);
				std::vector<long double> dec = rgb_to_dec(rgb);

				replace_all(t.data, "{{" + base + "-hex-r" + "}}", hex[0]);
				replace_all(t.data, "{{" + base + "-hex-g" + "}}", hex[1]);
				replace_all(t.data, "{{" + base + "-hex-b" + "}}", hex[2]);

				replace_all(t.data, "{{" + base + "-rgb-r" + "}}",
				            std::to_string(rgb[0]));
				replace_all(t.data, "{{" + base + "-rgb-g" + "}}",
				            std::to_string(rgb[1]));
				replace_all(t.data, "{{" + base + "-rgb-b" + "}}",
				            std::to_string(rgb[2]));

				replace_all(t.data, "{{" + base + "-dec-r" + "}}",
				            std::to_string(dec[0]));
				replace_all(t.data, "{{" + base + "-dec-g" + "}}",
				            std::to_string(dec[1]));
				replace_all(t.data, "{{" + base + "-dec-b" + "}}",
				            std::to_string(dec[2]));

				replace_all(t.data, "{{" + base + "-hex" + "}}", color);
				replace_all(t.data, "{{" + base + "-hex-bgr" + "}}",
				            hex[0] + hex[1] + hex[2]);
			}

			replace_all(t.data, "{{scheme-slug}}", s.slug);
			replace_all(t.data, "{{scheme-name}}", s.name);
			replace_all(t.data, "{{scheme-author}}", s.author);

			std::filesystem::path output_dir;

			if (make)
				output_dir = opt_build_dir / opt_output / t.name / t.output;
			else
				output_dir = opt_output / t.name / t.output;

			std::filesystem::create_directories(output_dir);
			std::ofstream output_file(output_dir / ("base16-" + s.slug + t.extension));

			if (output_file.good()) {
				output_file << t.data;
				output_file.close();
			}
		}
	}
}

auto
get_terminal_size() -> Terminal
{
	unsigned short width = 0;
	unsigned short height = 0;

#if defined(__linux__)
	struct winsize w {
		width, height
	};
	ioctl(fileno(stdout), TIOCGWINSZ, &w); // NOLINT (cppcoreguidelines-pro-type-vararg)
	width = w.ws_col;
	height = w.ws_row;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	width = (int)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
	height = (int)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
#endif

	Terminal size = {
		width,
		height,
	};

	return size;
}

void
list_templates(const std::filesystem::path &opt_cache_dir, const bool &opt_raw)
{
	std::vector<Template> templates = parse_template_dir(opt_cache_dir / "templates");

	if (opt_raw) {
		for (const Template &t : templates)
			std::cout << t.name << std::endl;
		return;
	}

	Terminal terminal_size = get_terminal_size();

	int index = 0;
	unsigned short length = 0;
	unsigned short num_of_element = templates.size();

	for (const Template &t : templates) {
		if (t.name.length() > length)
			length = t.name.length();
	}

	unsigned short num_of_column = terminal_size.width / (length + 1);
	unsigned short num_of_row = num_of_element / num_of_column;
	unsigned short remainder = num_of_element % num_of_column;
	unsigned short padding = length + 1;

	for (int row = 0; row < num_of_row; ++row) {
		if (row != 0)
			std::cout << std::endl;
		for (int column = 0; column < num_of_column; ++column) {
			std::cout << std::left << std::setw(padding) << templates[index].name
				  << std::setw(padding);
			index += 1;
		}
	}

	std::cout << std::endl;

	for (int column = 0; column < remainder; ++column) {
		std::cout << std::left << std::setw(padding) << templates[index].name
			  << std::setw(padding);
		index += 1;
	}

	std::cout << std::endl;
}

void
list_schemes(const std::filesystem::path &opt_cache_dir, const bool &opt_raw)
{
	std::vector<Scheme> schemes = parse_scheme_dir(opt_cache_dir / "schemes");

	if (opt_raw) {
		for (const Scheme &s : schemes)
			std::cout << s.slug << std::endl;
		return;
	}

	Terminal terminal_size = get_terminal_size();

	int index = 0;
	unsigned short length = 0;
	unsigned short num_of_element = schemes.size();

	for (const Scheme &s : schemes) {
		if (s.slug.length() > length)
			length = s.slug.length();
	}

	unsigned short num_of_column = terminal_size.width / (length + 1);
	unsigned short num_of_row = num_of_element / num_of_column;
	unsigned short remainder = num_of_element % num_of_column;
	unsigned short padding = length + 1;

	for (int row = 0; row < num_of_row; ++row) {
		if (row != 0)
			std::cout << std::endl;

		for (int column = 0; column < num_of_column; ++column) {
			std::cout << std::left << std::setw(padding) << schemes[index].slug
				  << std::setw(padding);
			index += 1;
		}
	}

	std::cout << std::endl;

	for (int column = 0; column < remainder; ++column) {
		std::cout << std::left << std::setw(padding) << schemes[index].slug
			  << std::setw(padding);
		index += 1;
	}

	std::cout << std::endl;
}

void
list(const std::filesystem::path &opt_cache_dir, const bool &opt_show_template,
     const bool &opt_show_scheme, const bool &opt_raw)
{
	if (opt_show_scheme && opt_show_template) {
		std::cout << "--- scheme ---" << std::endl;
		list_schemes(opt_cache_dir, opt_raw);
		std::cout << "--- template ---" << std::endl;
		list_templates(opt_cache_dir, opt_raw);
		return;
	}

	if (opt_show_scheme)
		list_schemes(opt_cache_dir, opt_raw);

	if (opt_show_template)
		list_templates(opt_cache_dir, opt_raw);
}

auto
main(int argc, char *argv[]) -> int
{
	std::span args(argv, size_t(argc));

	if (args[optind] == nullptr) {
		std::cout << "error: no command is detected" << std::endl;
		return 1;
	}

	int opt = 0;
	int index = 0;

	std::filesystem::path opt_cache_dir;

#if defined(__linux__)
	if (std::getenv("XDG_CACHE_HOME") != nullptr) // NOLINT (concurrency-mt-unsafe)
		opt_cache_dir /= std::getenv("XDG_CACHE_HOME"); // NOLINT (concurrency-mt-unsafe)
	else
		// NOLINTNEXTLINE (concurrency-mt-unsafe)
		opt_cache_dir /= (std::filesystem::path)std::getenv("HOME") / ".cache";
#elif defined(_WIN32)
	if (std::getenv("LOCALAPPDATA") != nullptr) // NOLINT (concurrency-mt-unsafe)
		opt_cache_dir /= std::getenv("LOCALAPPDATA"); // NOLINT (concurrency-mt-unsafe)
#endif

	opt_cache_dir /= "cbase16";

	if (!std::filesystem::is_directory(opt_cache_dir))
		std::filesystem::create_directory(opt_cache_dir);

	if (std::strcmp(args[optind], "update") == 0) {
		bool opt_legacy = false;

		while ((opt = getopt(argc, argv, "c:")) != EOF) { // NOLINT (concurrency-mt-unsafe)
			switch (opt) {
			case 'c':
				if (std::filesystem::is_directory(optarg)) {
					opt_cache_dir = optarg;
				} else {
					std::cout << "error: directory not found: " << optarg
						  << std::endl;
					return 1;
				}
				break;
			case 'l':
				opt_legacy = true;
				break;
			}
		}

		update(opt_cache_dir, opt_legacy);
	} else if (std::strcmp(args[optind], "build") == 0) {
		std::vector<std::string> opt_templates;
		std::vector<std::string> opt_schemes;
		std::filesystem::path opt_output = "base16-themes";

		// NOLINTNEXTLINE (concurrency-mt-unsafe)
		while ((opt = getopt(argc, argv, "c:t:s:o:")) != EOF) {
			switch (opt) {
			case 'c':
				if (std::filesystem::is_directory(optarg)) {
					opt_cache_dir = optarg;
				} else {
					std::cout << "error: directory not found: " << optarg
						  << std::endl;
					return 1;
				}
				break;
			case 't':
				index = optind - 1;
				while (index < argc) {
					std::string next = args[index];
					index++;
					if (next[0] != '-')
						opt_templates.emplace_back(next);
					else
						break;
				}
				break;
			case 's':
				index = optind - 1;
				while (index < argc) {
					std::string next = args[index];
					index++;
					if (next[0] != '-')
						opt_schemes.emplace_back(next);
					else
						break;
				}
				break;
			case 'o':
				opt_output = optarg;
				break;
			}
		}

		build(opt_cache_dir, opt_templates, opt_schemes, "", opt_output, false);
	} else if (std::strcmp(args[optind], "make") == 0) {
		std::vector<std::string> opt_templates;
		std::vector<std::string> opt_schemes;
		std::filesystem::path opt_build_dir = std::filesystem::current_path();
		std::filesystem::path opt_output = "base16-themes";

		// NOLINTNEXTLINE (concurrency-mt-unsafe)
		while ((opt = getopt(argc, argv, "c:C:t:s:o:")) != EOF) {
			switch (opt) {
			case 'c':
				if (std::filesystem::is_directory(optarg)) {
					opt_cache_dir = optarg;
				} else {
					std::cout << "error: directory not found: " << optarg
						  << std::endl;
					return 1;
				}
				break;
			case 'C':
				if (std::filesystem::is_directory(optarg)) {
					opt_build_dir = optarg;
				} else {
					std::cout << "error: directory not found: " << optarg
						  << std::endl;
					return 1;
				}
				break;
			case 't':
				index = optind - 1;
				while (index < argc) {
					std::string next = args[index];
					index++;
					if (next[0] != '-')
						opt_templates.emplace_back(next);
					else
						break;
				}
				break;
			case 's':
				index = optind - 1;
				while (index < argc) {
					std::string next = args[index];
					index++;
					if (next[0] != '-')
						opt_schemes.emplace_back(next);
					else
						break;
				}
				break;
			case 'o':
				opt_output = optarg;
				break;
			}
		}

		build(opt_cache_dir, opt_templates, opt_schemes, opt_build_dir, opt_output, true);
	} else if (std::strcmp(args[optind], "list") == 0) {
		bool opt_show_template = true;
		bool opt_show_scheme = true;
		bool opt_raw = false;

		while ((opt = getopt(argc, argv, "tsr")) != EOF) { // NOLINT (concurrency-mt-unsafe)
			switch (opt) {
			case 't':
				opt_show_scheme = false;
				opt_show_template = true;
				break;
			case 's':
				opt_show_scheme = true;
				opt_show_template = false;
				break;
			case 'r':
				opt_raw = true;
				break;
			}
		}

		list(opt_cache_dir, opt_show_template, opt_show_scheme, opt_raw);
	} else if (std::strcmp(args[optind], "version") == 0) {
		std::cout << "cbase16-0.5.3" << std::endl;
	} else if (std::strcmp(args[optind], "help") == 0) {
		std::cout << "usage: cbase16 [command] [options]\n\n"
			     "command:\n"
			     "   update  -- fetch all necessary sources for building\n"
			     "   build   -- generate colorscheme templates\n"
			     "   make    -- build current directory\n"
			     "   list    -- display available schemes and templates\n"
			     "   version -- display version\n"
			     "   help    -- display usage message\n\n"
			     "update options:\n"
			     "   -c -- specify cache directory\n\n"
			     "   -l -- use original base16 sources\n\n"
			     "build options:\n"
			     "   -c -- specify cache directory\n"
			     "   -s -- only build specified schemes\n"
			     "   -t -- only build specified templates\n"
			     "   -o -- specify output directory\n\n"
			     "make options:\n"
			     "   -c -- specify cache directory\n"
			     "   -C -- specify directory to build\n"
			     "   -s -- only build specified schemes\n"
			     "   -t -- only build specified templates\n"
			     "   -o -- specify output directory\n\n"
			     "list options:\n"
			     "   -s -- only show schemes\n"
			     "   -t -- only show templates\n"
			     "   -r -- list items in single column"
			  << std::endl;
	} else {
		std::cout << "error: invalid command: " << args[optind] << std::endl;
		return 1;
	}

	return 0;
}
