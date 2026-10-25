#include "native_ocr.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
void write_test_pdf(const std::filesystem::path& path) {
    const std::string content = "BT /F1 36 Tf 72 600 Td (MYCELIA PDF OCR 7391) Tj ET";
    const std::vector<std::string> objects{
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>",
        "<< /Length " + std::to_string(content.size()) + " >>\nstream\n" + content + "\nendstream",
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"
    };
    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";
    std::vector<std::streamoff> offsets;
    for (std::size_t index{}; index < objects.size(); ++index) {
        offsets.push_back(pdf.tellp());
        pdf << index + 1 << " 0 obj\n" << objects[index] << "\nendobj\n";
    }
    const auto xref = pdf.tellp();
    pdf << "xref\n0 " << objects.size() + 1 << "\n0000000000 65535 f \n";
    for (auto offset : offsets)
        pdf << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
    pdf << "trailer\n<< /Size " << objects.size() + 1 << " /Root 1 0 R >>\nstartxref\n"
        << xref << "\n%%EOF\n";
    std::ofstream output(path, std::ios::binary);
    output << pdf.str();
}
}

int main() {
    try {
        const auto path = std::filesystem::temp_directory_path() / "kr_mycelia_native_pdf_ocr.pdf";
        write_test_pdf(path);
        const auto result = krmemory::recognize_pdf_text(path);
        std::filesystem::remove(path);
        if (!result.available) {
            std::cout << "PASS: native PDF OCR capability cleanly unavailable: " << result.detail << '\n';
            return 0;
        }
        if (!result.success || result.text.find("MYCELIA") == std::string::npos ||
            result.text.find("7391") == std::string::npos)
            throw std::runtime_error("native PDF OCR did not recognize generated page: " + result.detail + " / " + result.text);
        if (result.lines.empty() || result.lines.front().page != 1)
            throw std::runtime_error("native PDF OCR page geometry missing");
        std::cout << "PASS: native Windows PDF rendering and OCR: " << result.text << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
