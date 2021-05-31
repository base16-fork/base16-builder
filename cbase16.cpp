#include <git2.h>
#include <yaml-cpp/yaml.h>

#include <map>
#include <filesystem>
#include <vector>
#include <cstring>
#include <fstream>
#include <iostream>

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

int do_clone(const char *, const char *);
void clone(std::string, std::string);
void emit_source(void);
void update(void);
std::vector<Template> get_templates(void);
std::vector<Scheme> get_schemes(void);
std::vector<int> hex_to_rgb(std::string);
void replace_all(std::string&, const std::string&, const std::string&);
void build(void);

int
do_clone(const char *path, const char *url)
{
	git_libgit2_init();
	git_repository *repo = NULL;
	int ret = git_clone(&repo, url, path, NULL);
	git_repository_free(repo);
	return ret;
}

void
clone(std::string dir, std::string source)
{
	std::ifstream file(source);
	if (file.is_open()) {
		YAML::Node file = YAML::LoadFile(source);

		std::vector<std::string> token_key;
		std::vector<std::string> token_value;

		for (YAML::const_iterator it = file.begin(); it != file.end();
		     ++it) {
			token_key.push_back(it->first.as<std::string>());
			token_value.push_back(it->second.as<std::string>());
		}

		#pragma omp parallel for
		for (int i = 0; i < token_key.size(); ++i) {
			do_clone((dir + token_key[i]).c_str(),
			         token_value[i].c_str());
		}
	} else {
		std::cerr << "error: cannot read " << source << std::endl;
		exit(1);
	}
	file.close();
}

void
emit_source(void)
{
	std::ofstream file("sources.yaml");

	if (file.is_open()) {
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
	} else {
		std::cerr
			<< "error: fail to write source.yaml to current directory";
		exit(1);
	}
	file.close();
}

void
update(void)
{
	emit_source();
	clone("./sources/", "sources.yaml");
	clone("./schemes/", "sources/schemes/list.yaml");
	clone("./templates/", "sources/templates/list.yaml");
}

std::vector<Template>
get_templates(void)
{
	std::vector<Template> templates;

	for (std::filesystem::directory_entry entry :
	     std::filesystem::directory_iterator("templates")) {
		std::string config_file =
			entry.path().string() + "/templates/config.yaml";
		std::string template_file =
			entry.path().string() + "/templates/default.mustache";

		Template tmp;
		YAML::Node config = YAML::LoadFile(config_file);
		std::ifstream templet(template_file);

		tmp.name = entry.path().stem().string();
		for (YAML::const_iterator it = config.begin();
		     it != config.end(); ++it) {
			YAML::Node node = config[it->first.as<std::string>()];
			if (node["extension"].Type() != YAML::NodeType::Null)
				tmp.extension =
					node["extension"].as<std::string>();
			else
				tmp.extension = "";

			tmp.output = node["output"].as<std::string>();
		}

		tmp.data = std::string(std::istreambuf_iterator<char>(templet),
		                       std::istreambuf_iterator<char>());

		templates.push_back(tmp);
		templet.close();
	}

	return templates;
}

std::vector<Scheme>
get_schemes(void)
{
	std::vector<Scheme> schemes;

	for (std::filesystem::directory_entry dir :
	     std::filesystem::directory_iterator("schemes")) {
		for (std::filesystem::directory_entry file :
		     std::filesystem::directory_iterator(dir)) {
			if (std::filesystem::is_regular_file(file) &&
			    file.path().extension() == ".yaml") {
				Scheme tmp;
				YAML::Node node =
					YAML::LoadFile(file.path().string());

				tmp.slug = file.path().stem().string();
				for (YAML::const_iterator it = node.begin();
				     it != node.end(); ++it) {
					std::string key =
						it->first.as<std::string>();
					std::string value =
						it->second.as<std::string>();

					if (key.compare("scheme") == 0)
						tmp.name = value;
					else if (key.compare("author") == 0)
						tmp.author = value;
					else
						tmp.colors.insert(
							{ key, value });
				}

				schemes.push_back(tmp);
			}
		}
	}

	return schemes;
}

std::vector<int>
hex_to_rgb(std::string hex)
{
	std::vector<int> rgb(3);
	std::stringstream ss;
	std::string str;

	int size = hex.size();

	for (int i = 0; i < 3; ++i) {
		if (hex.size() == 6)
			str = hex.substr(i * 2, 2);
		else
			break;

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
build(void)
{
	#pragma omp parallel for
	for (Scheme scheme : get_schemes()) {
		#pragma omp parallel for
		for (Template templet : get_templates()) {
			std::map<std::string, std::string> data;
			data["scheme-slug"] = scheme.slug;
			data["scheme-name"] = scheme.name;
			data["scheme-author"] = scheme.author;
			for (auto &[base, color] : scheme.colors) {
				std::vector<std::string> hex = {
					color.substr(0, 2), color.substr(2, 2),
					color.substr(4, 2)
				};
				std::vector<int> rgb = hex_to_rgb(color);

				data[base + "-hex-r"] = hex[0];
				data[base + "-rgb-r"] = std::to_string(rgb[0]);
				data[base + "-dec-r"] = std::to_string(
					(long double)rgb[0] / 255);

				replace_all(templet.data,
				            "{{" + base + "-hex-r" + "}}",
				            data[base + "-hex-r"]);
				replace_all(templet.data,
				            "{{" + base + "-rgb-r" + "}}",
				            data[base + "-rgb-r"]);
				replace_all(templet.data,
				            "{{" + base + "-dec-r" + "}}",
				            data[base + "-dec-r"]);

				data[base + "-hex-g"] = hex[1];
				data[base + "-rgb-g"] = std::to_string(rgb[1]);
				data[base + "-dec-g"] = std::to_string(
					(long double)rgb[1] / 255);

				replace_all(templet.data,
				            "{{" + base + "-hex-g" + "}}",
				            data[base + "-hex-g"]);
				replace_all(templet.data,
				            "{{" + base + "-rgb-g" + "}}",
				            data[base + "-rgb-g"]);
				replace_all(templet.data,
				            "{{" + base + "-dec-g" + "}}",
				            data[base + "-dec-g"]);

				data[base + "-hex-b"] = hex[2];
				data[base + "-rgb-b"] = std::to_string(rgb[2]);
				data[base + "-dec-b"] = std::to_string(
					(long double)rgb[2] / 255);

				replace_all(templet.data,
				            "{{" + base + "-hex-b" + "}}",
				            data[base + "-hex-b"]);
				replace_all(templet.data,
				            "{{" + base + "-rgb-b" + "}}",
				            data[base + "-rgb-b"]);
				replace_all(templet.data,
				            "{{" + base + "-dec-b" + "}}",
				            data[base + "-dec-b"]);

				data[base + "-hex"] = color;
				data[base + "-hex-bgr"] =
					hex[0] + hex[1] + hex[2];

				replace_all(templet.data,
				            "{{" + base + "-hex" + "}}",
				            data[base + "-hex"]);
				replace_all(templet.data,
				            "{{" + base + "-hex-bgr" + "}}",
				            data[base + "-hex-bgr"]);
			}

			replace_all(templet.data, "{{scheme-name}}",
			            scheme.name);
			replace_all(templet.data, "{{scheme-author}}",
			            scheme.name);

			std::string output_dir =
				"output/" + templet.name + "/" + templet.output;
			std::filesystem::create_directories(output_dir);
			std::ofstream output_file(output_dir + "/base16-" +
			                          scheme.slug +
			                          templet.extension);
			output_file << templet.data << std::endl;
			output_file.close();
		}
	}
}

int
main(int argc, char *argv[])
{
	if (argc == 1) {
		std::cerr << "error: empty command" << std::endl;
		return 1;
	} else if (argc > 2) {
		std::cerr << "error: too many commands" << std::endl;
		return 1;
	}

	if (strcmp(argv[1], "update") == 0) {
		update();
	} else if (strcmp(argv[1], "build") == 0) {
		build();
	} else if (strcmp(argv[1], "help") == 0) {
		std::cout << "Usage: cbase16 [command]" << std::endl << std::endl;
		std::cout << "Command:" << std::endl;
		std::cout << "    update -- fetch all necessary sources for building" << std::endl;
		std::cout << "    build  -- generate colorscheme templates" << std::endl;
		std::cout << "    help   -- display usage message" << std::endl;
	} else {
		std::cerr << "error: invalid command" << std::endl;
		return 1;
	}

	return 0;
}
