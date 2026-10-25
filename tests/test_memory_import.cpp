#include "memory_import.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void u16(std::string& output, unsigned value) {
    output += char(value);
    output += char(value >> 8);
}

static void u32(std::string& output, unsigned value) {
    u16(output, value);
    u16(output, value >> 16);
}

static std::string bytes_from_hex(std::string_view hex) {
    if (hex.size() % 2) throw std::runtime_error("invalid test hex");
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index{}; index < hex.size(); index += 2) {
        const auto nibble = [](char value) -> unsigned {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            throw std::runtime_error("invalid test hex digit");
        };
        bytes += static_cast<char>((nibble(hex[index]) << 4) | nibble(hex[index + 1]));
    }
    return bytes;
}

static void office_archive(const fs::path& file, std::string_view name, std::string_view xml, std::string payload, unsigned method) {
    std::string zip;
    u32(zip, 0x04034b50); u16(zip, 20); u16(zip, 0); u16(zip, method); u16(zip, 0); u16(zip, 0);
    u32(zip, 0); u32(zip, static_cast<unsigned>(payload.size())); u32(zip, static_cast<unsigned>(xml.size()));
    u16(zip, static_cast<unsigned>(name.size())); u16(zip, 0); zip += name; zip += payload;
    const auto central = static_cast<unsigned>(zip.size());
    u32(zip, 0x02014b50); u16(zip, 20); u16(zip, 20); u16(zip, 0); u16(zip, method); u16(zip, 0); u16(zip, 0);
    u32(zip, 0); u32(zip, static_cast<unsigned>(payload.size())); u32(zip, static_cast<unsigned>(xml.size()));
    u16(zip, static_cast<unsigned>(name.size())); u16(zip, 0); u16(zip, 0); u16(zip, 0); u16(zip, 0); u32(zip, 0); u32(zip, 0); zip += name;
    const auto central_size = static_cast<unsigned>(zip.size()) - central;
    u32(zip, 0x06054b50); u16(zip, 0); u16(zip, 0); u16(zip, 1); u16(zip, 1); u32(zip, central_size); u32(zip, central); u16(zip, 0);
    std::ofstream output(file, std::ios::binary);
    output.write(zip.data(), static_cast<std::streamsize>(zip.size()));
}

int main() {
    try {
        const auto directory = fs::temp_directory_path() / "kr_mycelia_import_project";
        fs::create_directories(directory);
        const auto markdown = directory / "notes.md";
        { std::ofstream output(markdown); output << "# Native memory\nPersist this source content."; }
        const std::string stored_xml = "<w:document><w:body><w:p><w:t>Office memory content</w:t></w:p></w:body></w:document>";
        const std::string deflated_xml = "<w:document><w:body><w:p><w:t>Compressed Office memory content</w:t></w:p></w:body></w:document>";
        const auto stored = directory / "stored.docx";
        const auto deflated = directory / "deflated.docx";
        const auto spreadsheet = directory / "sheet.xlsx";
        const auto presentation = directory / "slides.pptx";
        office_archive(stored, "word/document.xml", stored_xml, stored_xml, 0);
        office_archive(deflated, "word/document.xml", deflated_xml,
                       bytes_from_hex("B329B74AC94F2ECD4DCD2BB1B329B74ACA4FA904D10520A2C4CE393FB7A028B5B8383545C13F2D2D33395521373537BFA85221393FAF2435AFC4461FA40A4416804988767D243301"), 8);
        const std::string spreadsheet_xml = "<sst><si><t>Spreadsheet memory content</t></si></sst>";
        const std::string presentation_xml = "<p:sld><a:t>Presentation memory content</a:t></p:sld>";
        office_archive(spreadsheet, "xl/sharedStrings.xml", spreadsheet_xml, spreadsheet_xml, 0);
        office_archive(presentation, "ppt/slides/slide1.xml", presentation_xml, presentation_xml, 0);

        mycelia::DynamicAssociativeDatabase database;
        auto markdown_result = krmemory::import_file(database, markdown);
        auto stored_result = krmemory::import_file(database, stored);
        auto deflated_result = krmemory::import_file(database, deflated);
        auto spreadsheet_result = krmemory::import_file(database, spreadsheet);
        auto presentation_result = krmemory::import_file(database, presentation);
        check(markdown_result.format == ".md", "Markdown format missing");
        check(stored_result.format == ".docx" && deflated_result.format == ".docx", "Office format missing");
        check(spreadsheet_result.format == ".xlsx" && presentation_result.format == ".pptx", "Spreadsheet or presentation format missing");
        const auto entries = krmemory::list_collection(database, "kr_mycelia_import_project");
        check(entries.size() == 5, "Collection membership missing");
        bool found_stored{}, found_deflated{}, found_spreadsheet{}, found_presentation{};
        for (const auto& entry : entries) {
            const auto content = entry.data.find("content");
            if (content == entry.data.end()) continue;
            const auto text = mycelia::sql::display(content->second);
            found_stored |= text.find("Office memory content") != std::string::npos;
            found_deflated |= text.find("Compressed Office memory content") != std::string::npos;
            found_spreadsheet |= text.find("Spreadsheet memory content") != std::string::npos;
            found_presentation |= text.find("Presentation memory content") != std::string::npos;
        }
        check(found_stored, "Stored Office XML content missing");
        check(found_deflated, "Deflated Office XML content missing");
        check(found_spreadsheet, "Spreadsheet XML content missing");
        check(found_presentation, "Presentation XML content missing");

        mycelia::DynamicAssociativeDatabase cancelled_database;
        std::atomic_bool cancel{true};
        bool cancellation_reported{};
        try {
            (void)krmemory::import_file(cancelled_database, markdown, &cancel);
        } catch (const std::runtime_error& error) {
            cancellation_reported = std::string_view(error.what()) == "import_cancelled";
        }
        check(cancellation_reported, "Import cancellation was not reported");
        check(cancelled_database.external_record_count() == 0, "Cancelled import wrote partial DAD records");
        fs::remove_all(directory);
        std::cout << "PASS: native text/Office imports and atomic pre-write cancellation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
