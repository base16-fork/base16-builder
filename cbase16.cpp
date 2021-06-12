#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <unistd.h>
#include <vector>

#include <git2.h>
#include <yaml-cpp/yaml.h>

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

std::vector<std::string> opt_schemes;
std::vector<std::string> opt_templates;
std::filesystem::path opt_output = "output";
std::filesystem::path opt_cache_dir;

void clone(std::string &, std::string &);
void update();
auto get_templates() -> std::vector<Template>;
auto get_schemes() -> std::vector<Scheme>;
auto hex_to_rgb(std::string &) -> std::vector<int>;
void replace_all(std::string &, const std::string &, const std::string &);
void build();

void
clone(const std::string &dir, const std::string &source)
{
	if (std::filesystem::is_regular_file(source)) {
		YAML::Node file = YAML::LoadFile(source);

		std::vector<std::string> token_key;
		std::vector<std::string> token_value;

		for (YAML::const_iterator it = file.begin(); it != file.end(); ++it) {
			token_key.push_back(it->first.as<std::string>());
			token_value.push_back(it->second.as<std::string>());
		}

#pragma omp parallel for
		for (int i = 0; i < token_key.size(); ++i) {
			git_repository *repo = nullptr;
			git_clone(&repo, token_value[i].c_str(),
			          (opt_cache_dir / dir / token_key[i]).c_str(), nullptr);
			git_repository_free(repo);
		}

	} else {
		std::cout << "error: cannot read " + source << std::endl;
		exit(1);
	}
}

void
update()
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
		exit(1);
	}

	git_libgit2_init();
	clone("sources", opt_cache_dir / "sources.yaml");
	clone("schemes", opt_cache_dir / "sources" / "schemes" / "list.yaml");
	clone("templates", opt_cache_dir / "sources" / "templates" / "list.yaml");
	git_libgit2_shutdown();
}

auto
get_templates() -> std::vector<Template>
{
	std::vector<Template> templates;

	if (!std::filesystem::is_directory(opt_cache_dir / "templates")) {
		std::cout << "warning: cache template directory is either empty or not found"
			  << std::endl;
		return templates;
	}

	for (std::filesystem::directory_entry entry :
	     std::filesystem::directory_iterator(opt_cache_dir / "templates")) {
		std::string config_file = entry.path() / "templates" / "config.yaml";
		std::string template_file = entry.path() / "templates" / "default.mustache";

		YAML::Node config = YAML::LoadFile(config_file);
		std::ifstream templet(template_file, std::ios::binary | std::ios::ate);

		Template t;
		t.name = entry.path().stem().string();
		for (YAML::const_iterator it = config.begin(); it != config.end(); ++it) {
			YAML::Node node = config[it->first.as<std::string>()];
			if (node["extension"].Type() != YAML::NodeType::Null)
				t.extension = node["extension"].as<std::string>();
			else
				t.extension = "";

			t.output = node["output"].as<std::string>();
		}

		if (templet.good()) {
			templet.seekg(0, std::ios::end);
			t.data.resize(templet.tellg());
			templet.seekg(0, std::ios::beg);
			templet.read(&t.data[0], t.data.size());
			templet.close();
		}

		templates.push_back(t);
	}

	return templates;
}

auto
get_schemes() -> std::vector<Scheme>
{
	std::vector<Scheme> schemes;

	if (!std::filesystem::is_directory(opt_cache_dir / "schemes")) {
		std::cout << "warning: cache scheme directory is either empty or not found"
			  << std::endl;
		return schemes;
	}

	for (std::filesystem::directory_entry dir :
	     std::filesystem::directory_iterator(opt_cache_dir / "schemes")) {
		for (std::filesystem::directory_entry file :
		     std::filesystem::directory_iterator(dir)) {
			if (file.is_regular_file() && file.path().extension() == ".yaml") {
				YAML::Node node = YAML::LoadFile(file.path().string());

				Scheme s;
				s.slug = file.path().stem().string();
				for (YAML::const_iterator it = node.begin(); it != node.end();
				     ++it) {
					std::string key = it->first.as<std::string>();
					std::string value = it->second.as<std::string>();

					if (key.compare("scheme") == 0)
						s.name = value;
					else if (key.compare("author") == 0)
						s.author = value;
					else
						s.colors.insert({ key, value });
				}

				schemes.push_back(s);
			}
		}
	}

	return schemes;
}

auto
hex_to_rgb(std::string &hex) -> std::vector<int>
{
	std::vector<int> rgb(3);
	std::stringstream ss;
	std::string str;

	if (hex.size() != 6)
		return rgb;

#pragma omp parallel for
	for (int i = 0; i < 3; ++i) {
		str = hex.substr(i * 2, 2);
		ss << std::hex << str;
		ss >> rgb[i];
		ss.clear();
	}

	return rgb;
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
build()
{
	std::vector<Scheme> schemes = get_schemes();
	std::vector<Template> templates = get_templates();

#pragma omp parallel for
	for (Scheme s : schemes) {
		if (!opt_schemes.empty() &&
		    std::find(opt_schemes.begin(), opt_schemes.end(), s.slug) == opt_schemes.end())
			continue;

#pragma omp parallel for
		for (Template t : templates) {
			if (!opt_templates.empty() &&
			    std::find(opt_templates.begin(), opt_templates.end(), t.name) ==
			            opt_templates.end())
				continue;

			std::map<std::string, std::string> data;
			data["scheme-slug"] = s.slug;
			data["scheme-name"] = s.name;
			data["scheme-author"] = s.author;
			for (auto &[base, color] : s.colors) {
				std::vector<std::string> hex = { color.substr(0, 2),
					                         color.substr(2, 2),
					                         color.substr(4, 2) };
				std::vector<int> rgb = hex_to_rgb(color);

				data[base + "-hex-r"] = hex[0];
				data[base + "-rgb-r"] = std::to_string(rgb[0]);
				data[base + "-dec-r"] = std::to_string((long double)rgb[0] / 255);

				replace_all(t.data, "{{" + base + "-hex-r" + "}}",
				            data[base + "-hex-r"]);
				replace_all(t.data, "{{" + base + "-rgb-r" + "}}",
				            data[base + "-rgb-r"]);
				replace_all(t.data, "{{" + base + "-dec-r" + "}}",
				            data[base + "-dec-r"]);

				data[base + "-hex-g"] = hex[1];
				data[base + "-rgb-g"] = std::to_string(rgb[1]);
				data[base + "-dec-g"] = std::to_string((long double)rgb[1] / 255);

				replace_all(t.data, "{{" + base + "-hex-g" + "}}",
				            data[base + "-hex-g"]);
				replace_all(t.data, "{{" + base + "-rgb-g" + "}}",
				            data[base + "-rgb-g"]);
				replace_all(t.data, "{{" + base + "-dec-g" + "}}",
				            data[base + "-dec-g"]);

				data[base + "-hex-b"] = hex[2];
				data[base + "-rgb-b"] = std::to_string(rgb[2]);
				data[base + "-dec-b"] = std::to_string((long double)rgb[2] / 255);

				replace_all(t.data, "{{" + base + "-hex-b" + "}}",
				            data[base + "-hex-b"]);
				replace_all(t.data, "{{" + base + "-rgb-b" + "}}",
				            data[base + "-rgb-b"]);
				replace_all(t.data, "{{" + base + "-dec-b" + "}}",
				            data[base + "-dec-b"]);

				data[base + "-hex"] = color;
				data[base + "-hex-bgr"] = hex[0] + hex[1] + hex[2];

				replace_all(t.data, "{{" + base + "-hex" + "}}",
				            data[base + "-hex"]);
				replace_all(t.data, "{{" + base + "-hex-bgr" + "}}",
				            data[base + "-hex-bgr"]);
			}

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
main(int argc, char *argv[]) -> int
{
	if (std::getenv("XDG_CACHE_HOME") != nullptr)
		opt_cache_dir /= std::getenv("XDG_CACHE_HOME");
	else if (std::getenv("LOCALAPPDATA") != nullptr)
		opt_cache_dir /= std::getenv("LOCALAPPDATA");
	else
		opt_cache_dir /= (std::filesystem::path)std::getenv("HOME") / ".cache";

	opt_cache_dir /= "cbase16";

	if (!std::filesystem::is_directory(opt_cache_dir))
		std::filesystem::create_directory(opt_cache_dir);

	std::span args(argv, size_t(argc));

	int opt = 0, index = 0;
	while ((opt = getopt(argc, argv, "c:s:t:o:")) != EOF) {
		switch (opt) {
		case 'c':
			opt_cache_dir = optarg;
			break;
		case 's':
			index = optind - 1;
			while (index < argc) {
				std::string next = args[index];
				index++;
				if (next[0] != '-')
					opt_schemes.push_back(next);
				else
					break;
			}
			break;
		case 't':
			index = optind - 1;
			while (index < argc) {
				std::string next = args[index];
				index++;
				if (next[0] != '-')
					opt_templates.push_back(next);
				else
					break;
			}
			break;
		case 'o':
			opt_output = optarg;
			break;
		}
	}

	if (args[optind] == nullptr) {
		std::cout << "error: no command is detected" << std::endl;
		return 1;
	}

	if (std::strcmp(args[optind], "update") == 0) {
		update();
	} else if (std::strcmp(args[optind], "build") == 0) {
		build();
	} else if (std::strcmp(args[optind], "version") == 0) {
		std::cout << "cbase16-0.3.0" << std::endl;
	} else if (std::strcmp(args[optind], "help") == 0) {
		std::cout << "usage: cbase16 [command] [options]\n"
			     "command:\n"
			     "   update  -- fetch all necessary sources for building\n"
			     "   build   -- generate colorscheme templates\n"
			     "   version -- display version\n"
			     "   help    -- display usage message\n"
			     "options:\n"
			     "   -c -- specify cache directory\n"
			     "   -s -- only build specified schemes\n"
			     "   -t -- only build specified templates\n"
			     "   -o -- specify output directory"
			  << std::endl;
	} else {
		std::cout << "error: invalid command: " << args[optind] << std::endl;
		return 1;
	}

	return 0;
}
