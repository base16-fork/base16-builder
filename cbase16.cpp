#include <git2.h>
#include <yaml-cpp/yaml.h>
#include <boost/filesystem.hpp>

#include <map>
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

static int do_clone(const char *, const char *);
static void clone(std::string, std::string);
static void emit_source(void);
static std::vector<Template> get_templates(void);
static std::vector<Scheme> get_schemes(void);
static void update(void);

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

	for (boost::filesystem::directory_entry &entry :
	     boost::filesystem::directory_iterator("templates")) {
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

	for (boost::filesystem::directory_entry &dir :
	     boost::filesystem::directory_iterator("schemes")) {
		for (boost::filesystem::directory_entry &file :
		     boost::filesystem::directory_iterator(dir)) {
			if (boost::filesystem::is_regular_file(file) &&
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
	} else {
		std::cerr << "error: invalid command" << std::endl;
		return 1;
	}

	return 0;
}
