#include "../include/httplib.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>

std::mutex log_mutex;
std::string server_log;

void add_log(const std::string &entry) {
    std::lock_guard<std::mutex> lock(log_mutex);
    server_log += entry + "\n";
    std::cout << entry << std::endl;
}

int main() {
    if (geteuid() != 0)
    {
        std::cout << "Not root!" << std::endl;
        return 1;
    }
    httplib::Server svr;

    // 1. /info
    svr.Get("/info", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("All good", "text/plain");
    });

    // 2. /log
    svr.Get("/log", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(server_log.empty() ? "Log is empty" : server_log, "text/plain");
    });

    // 3. /upload
    svr.Post("/upload", [](const httplib::Request &req, httplib::Response &res) {
        if (!req.is_multipart_form_data()) {
            res.set_content("Expected multipart/form-data\n", "text/plain");
            return;
        }

        auto &form = req.form;

        if (form.files.empty()) {
            res.set_content("No files uploaded\n", "text/plain");
            return;
        }

        for (const auto & [field_name, file] : form.files) {
            std::string filename = file.filename;
            auto pos = filename.find_last_of("/\\");
            if (pos != std::string::npos) filename = filename.substr(pos + 1);

            std::string save_path = "/tmp/" + filename;

            std::ofstream ofs(save_path, std::ios::binary);
            if (!ofs) {
                add_log("Failed to save file: " + filename);
                res.set_content("Failed to save file\n", "text/plain");
                return;
            }

            ofs.write(file.content.c_str(), file.content.size());
            ofs.close();

            add_log("Uploaded file: " + filename + " (" + std::to_string(file.content.size()) + " b)");
        }

        res.set_content("File uploaded successfully\n", "text/plain");
    });

    std::cout << "Server on port 1616...\n" << std::endl;
    svr.listen("0.0.0.0", 1616);

    return 0;
}
