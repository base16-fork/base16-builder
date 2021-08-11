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

constexpr int HEX_MIN_LENGTH = 6;
constexpr int RGB_MIN_SIZE = 3;
constexpr int RGB_DEC = 255;

void clone(const std::filesystem::path &, const std::string &, const std::string &);
void update(const std::filesystem::path &);
auto get_templates(const std::filesystem::path &) -> std::vector<Template>;
auto get_schemes(const std::filesystem::path &) -> std::vector<Scheme>;
auto hex_to_rgb(const std::string &) -> std::vector<int>;
auto rgb_to_dec(const std::vector<int> &) -> std::vector<long double>;
void replace_all(std::string &, const std::string &, const std::string &);
void build(const std::filesystem::path &, const std::vector<std::string> &,
	   const std::vector<std::string> &, const std::filesystem::path &);
auto get_terminal_size() -> std::vector<unsigned short>;
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
update(const std::filesystem::path &opt_cache_dir)
{
	std::ofstream file(opt_cache_dir / "sources.yaml");

	if (file.good()) {
		YAML::Emitter source;
		source << YAML::BeginMap;
		source << YAML::Key << "schemes";
		source << YAML::Value
		       << "https://github.com/chriskempson/base16-schemes-source.git";
		source << YAML::Key << "templates";
		source << YAML::Value
		       << "https://github.com/chriskempson/base16-templates-source.git";
		source << YAML::EndMap;

		file << source.c_str();
		file.close();
	} else {
		std::cout << "error: fail to write sources.yaml to " << opt_cache_dir << std::endl;
		_exit(1);
	}

	git_libgit2_init();
	clone(opt_cache_dir, "sources", opt_cache_dir / "sources.yaml");
	clone(opt_cache_dir, "schemes", opt_cache_dir / "sources" / "schemes" / "list.yaml");
	clone(opt_cache_dir, "templates", opt_cache_dir / "sources" / "templates" / "list.yaml");
	git_libgit2_shutdown();
}

auto
get_templates(const std::filesystem::path &directory) -> std::vector<Template>
{
	std::vector<Template> templates;

	if (!std::filesystem::is_directory(directory)) {
		std::cout << "warning: cache template directory is either empty or not found"
			  << std::endl;
		return templates;
	}

	for (const std::filesystem::directory_entry &entry :
	     std::filesystem::directory_iterator(directory)) {
		if (!std::filesystem::is_regular_file(entry.path() / "templates" / "config.yaml")) {
			std::cout << "error: cannot get config file for " << entry.path()
				  << std::endl;
			continue;
		}
		for (const std::filesystem::directory_entry &file :
		     std::filesystem::directory_iterator(entry.path() / "templates")) {
			if (file.path().extension() == ".mustache") {
				Template t;

				YAML::Node config =
					YAML::LoadFile(entry.path() / "templates" / "config.yaml");
				t.name = entry.path().stem().string();
				for (YAML::const_iterator it = config.begin(); it != config.end();
				     ++it) {
					YAML::Node node = config[it->first.as<std::string>()];
					if (it->first.as<std::string>() ==
					    file.path().stem().string()) {
						if (node["extension"].Type() !=
						    YAML::NodeType::Null)
							t.extension =
								node["extension"].as<std::string>();

						t.output = node["output"].as<std::string>();
					}
				}

				std::ifstream templet(file.path().string(),
						      std::ios::binary | std::ios::ate);
				if (templet.good()) {
					templet.seekg(0, std::ios::end);
					t.data.resize(templet.tellg());
					templet.seekg(0, std::ios::beg);
					templet.read(&t.data[0], (long)t.data.size());
					templet.close();
				}

				templates.emplace_back(t);
			}
		}
	}

	return templates;
}

auto
get_schemes(const std::filesystem::path &directory) -> std::vector<Scheme>
{
	std::vector<Scheme> schemes;

	if (!std::filesystem::is_directory(directory)) {
		std::cout << "warning: cache scheme directory is either empty or not found"
			  << std::endl;
		return schemes;
	}

	for (const std::filesystem::directory_entry &dir :
	     std::filesystem::directory_iterator(directory)) {
		for (const std::filesystem::directory_entry &file :
		     std::filesystem::directory_iterator(dir)) {
			if (file.is_regular_file() && file.path().extension() == ".yaml") {
				YAML::Node node = YAML::LoadFile(file.path().string());

				Scheme s;
				s.slug = file.path().stem().string();
				for (YAML::const_iterator it = node.begin(); it != node.end();
				     ++it) {
					auto key = it->first.as<std::string>();
					auto value = it->second.as<std::string>();

					if (key == "scheme")
						s.name = value;
					else if (key == "author")
						s.author = value;
					else
						s.colors.insert({ key, value });
				}

				schemes.emplace_back(s);
			}
		}
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
      const std::vector<std::string> &opt_schemes, const std::filesystem::path &opt_output)
{
	std::vector<Template> templates;
	std::vector<Scheme> schemes;

	schemes = get_schemes(opt_cache_dir / "schemes");
	templates = get_templates(opt_cache_dir / "templates");

#pragma omp parallel for default(none) \
	shared(opt_templates, opt_schemes, templates, schemes, opt_output)
	for (const Scheme &s : schemes) {
		if (!opt_schemes.empty() &&
		    std::find(opt_schemes.begin(), opt_schemes.end(), s.slug) == opt_schemes.end())
			continue;

#pragma omp parallel for default(none) shared(opt_templates, templates, s, opt_output)
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

			std::filesystem::path output_dir = opt_output / t.name / t.output;
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
get_terminal_size() -> std::vector<unsigned short>
{
	unsigned short width = 0;
	unsigned short height = 0;
	std::vector<unsigned short> size(2);

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

	size[0] = width;
	size[1] = height;

	return size;
}

void
list_templates(const std::filesystem::path &opt_cache_dir, const bool &opt_raw)
{
	std::vector<Template> templates = get_templates(opt_cache_dir / "templates");

	if (opt_raw) {
		for (const Template &t : templates)
			std::cout << t.name << std::endl;
		return;
	}

	std::vector<unsigned short> terminal_size = get_terminal_size();

	int index = 0;
	unsigned short length = 0;
	unsigned short num_of_element = templates.size();

	for (const Template &t : templates) {
		if (t.name.length() > length)
			length = t.name.length();
	}

	unsigned short num_of_column = terminal_size[0] / (length + 1);
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
	std::vector<Scheme> schemes = get_schemes(opt_cache_dir / "schemes");

	if (opt_raw) {
		for (const Scheme &s : schemes)
			std::cout << s.slug << std::endl;
		return;
	}

	std::vector<unsigned short> terminal_size = get_terminal_size();

	int index = 0;
	unsigned short length = 0;
	unsigned short num_of_element = schemes.size();

	for (const Scheme &s : schemes) {
		if (s.slug.length() > length)
			length = s.slug.length();
	}

	unsigned short num_of_column = terminal_size[0] / (length + 1);
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
		while ((opt = getopt(argc, argv, "c")) != EOF) { // NOLINT (concurrency-mt-unsafe)
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
			}
		}
		update(opt_cache_dir);
	} else if (std::strcmp(args[optind], "build") == 0) {
		std::vector<std::string> opt_templates;
		std::vector<std::string> opt_schemes;
		std::filesystem::path opt_output = "output";

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
		build(opt_cache_dir, opt_templates, opt_schemes, opt_output);
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
		std::cout << "cbase16-0.5.1" << std::endl;
	} else if (std::strcmp(args[optind], "help") == 0) {
		std::cout << "usage: cbase16 [command] [options]\n\n"
			     "command:\n"
			     "   update  -- fetch all necessary sources for building\n"
			     "   build   -- generate colorscheme templates\n"
			     "   list    -- display available schemes and templates\n"
			     "   version -- display version\n"
			     "   help    -- display usage message\n\n"
			     "update options:\n"
			     "   -c -- specify cache directory\n\n"
			     "build options:\n"
			     "   -c -- specify cache directory\n"
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
