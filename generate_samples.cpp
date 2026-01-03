// Generate sample files for documentation
// Usage: ./generate_samples
// Creates samples/input/*.txt and samples/output/*.mzip

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include "mzip.hpp"
#include "generators.hpp"

void write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void write_file(const std::string& path, const uint8_t* data, size_t size) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data), size);
}

int main() {
    // All benchmark sizes
    std::vector<std::pair<std::string, size_t>> sizes = {
        {"64k", 65536},
        {"256k", 262144},
        {"1m", 1048576},
    };

    struct Sample {
        std::string name;
        std::string ext;
        std::function<std::vector<uint8_t>(size_t)> gen;
    };

    std::vector<Sample> samples = {
        // NUMERIC - show the patterns
        {"timestamps_regular", "bin", gen::timestamps_regular},
        {"db_ids", "bin", gen::db_ids},
        {"int_array", "bin", gen::int_array},
        {"gps_track", "bin", gen::gps_track},
        {"floats", "bin", gen::floats},
        {"sensor_16bit", "bin", gen::sensor_16bit},

        // STRUCTURED - text formats
        {"graphql", "graphql", gen::graphql},
        {"sql", "sql", gen::sql},
        {"json", "json", gen::json},
        {"xml", "xml", gen::xml},
        {"csv", "csv", gen::csv},
        {"base64", "txt", gen::base64},

        // CODE
        {"javascript", "js", gen::javascript},
        {"python", "py", gen::python},
        {"typescript", "ts", gen::typescript},
        {"html", "html", gen::html},
        {"css", "css", gen::css},
        {"golang", "go", gen::golang},
        {"rust", "rs", gen::rust},
        {"java", "java", gen::java},
        {"c", "c", gen::c_code},
        {"bash", "sh", gen::bash_script},
        {"php", "php", gen::php},

        // CONFIG
        {"docker_compose", "yaml", gen::docker_compose},
        {"terraform", "tf", gen::terraform},
        {"yaml_config", "yaml", gen::yaml_config},
        {"toml_config", "toml", gen::toml_config},
        {"env_file", "env", gen::env_file},
        {"ini_config", "ini", gen::ini_config},

        // BUILD
        {"package_json", "json", gen::package_json},
        {"cargo_toml", "toml", gen::cargo_toml},
        {"makefile", "makefile", gen::makefile},

        // LOG
        {"access_log", "log", gen::access_log},
        {"nginx_log", "log", gen::nginx_log},
        {"syslog", "log", gen::syslog},

        // BINARY
        {"image", "bin", gen::image},
        {"audio", "bin", gen::audio},
        {"executable", "bin", gen::executable},
        {"sparse_bitmap", "bin", gen::sparse_bitmap},

        // ADDITIONAL
        {"markdown", "md", gen::markdown},
        {"metrics", "txt", gen::metrics},
        {"k8s_manifest", "yaml", gen::k8s_manifest},
        {"email_headers", "txt", gen::email_headers},
        {"protobuf_like", "bin", gen::protobuf_like},
        {"unicode_text", "txt", gen::unicode_text},
        {"natural_text", "txt", gen::natural_text},
    };

    std::cout << "Generating " << samples.size() << " types x " << sizes.size() << " sizes = "
              << samples.size() * sizes.size() << " sample files...\n\n";

    for (const auto& [size_name, size_bytes] : sizes) {
        std::cout << "=== " << size_name << " (" << size_bytes << " bytes) ===\n";

        for (const auto& s : samples) {
            // Generate input
            auto data = s.gen(size_bytes);
            std::string input_path = "samples/" + size_name + "/input/" + s.name + "." + s.ext;
            write_file(input_path, data);

            // Compress
            auto compressed = mzip::compress(data.data(), data.size());
            std::string output_path = "samples/" + size_name + "/output/" + s.name + ".mzip";
            write_file(output_path, compressed);

            // Stats
            double ratio = (double)data.size() / compressed.size();
            printf("  %-20s %7zu -> %7zu (%7.1fx)\n",
                   s.name.c_str(), data.size(), compressed.size(), ratio);
        }
        std::cout << "\n";
    }

    std::cout << "Done! Files in samples/{64k,256k,1m}/{input,output}/\n";
    return 0;
}
