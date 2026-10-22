//-----------------------------------------------------------------------------
// Export a 3MF file (3D Manufacturing Format, core specification): one object per part, each
// triangle in its colour. The package is a zip; its parts are stored uncompressed, so no
// compression library is needed.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

static uint32_t Crc32(const std::string &data) {
    static uint32_t table[256];
    static bool     made = false;
    if(!made) {
        for(uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for(int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        made = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for(unsigned char b : data) crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static void Put16(std::string *s, uint32_t v) {
    s->push_back((char)(v & 0xFF));
    s->push_back((char)((v >> 8) & 0xFF));
}

static void Put32(std::string *s, uint32_t v) {
    Put16(s, v & 0xFFFF);
    Put16(s, v >> 16);
}

// A zip archive of the named files, stored; false if it is too big for a 32-bit zip.
static bool Zip(const std::vector<std::pair<std::string, std::string>> &files, std::string *out) {
    std::string central;
    for(const auto &file : files) {
        if(file.second.size() >= 0xFFFFFFFFu || out->size() >= 0xFFFFFFFFu) return false;
        uint32_t crc = Crc32(file.second), size = (uint32_t)file.second.size();
        uint32_t offset = (uint32_t)out->size();
        // Local header: version 2.0, no flags, stored, 1980-01-01 00:00.
        Put32(out, 0x04034b50);
        Put16(out, 20);
        Put16(out, 0);
        Put16(out, 0);
        Put16(out, 0);
        Put16(out, 0x21);
        Put32(out, crc);
        Put32(out, size);
        Put32(out, size);
        Put16(out, (uint32_t)file.first.size());
        Put16(out, 0);
        *out += file.first;
        *out += file.second;
        // Its central directory entry.
        Put32(&central, 0x02014b50);
        Put16(&central, 20);
        Put16(&central, 20);
        Put16(&central, 0);
        Put16(&central, 0);
        Put16(&central, 0);
        Put16(&central, 0x21);
        Put32(&central, crc);
        Put32(&central, size);
        Put32(&central, size);
        Put16(&central, (uint32_t)file.first.size());
        Put16(&central, 0);
        Put16(&central, 0);
        Put16(&central, 0);
        Put16(&central, 0);
        Put32(&central, 0);
        Put32(&central, offset);
        central += file.first;
    }
    if(out->size() + central.size() >= 0xFFFFFFFFu) return false;
    uint32_t start = (uint32_t)out->size();
    *out += central;
    Put32(out, 0x06054b50);
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, (uint32_t)files.size());
    Put16(out, (uint32_t)files.size());
    Put32(out, (uint32_t)central.size());
    Put32(out, start);
    Put16(out, 0);
    return true;
}

static std::string XmlEscape(const std::string &s) {
    std::string r;
    for(char c : s) {
        switch(c) {
            case '&':  r += "&amp;";  break;
            case '<':  r += "&lt;";   break;
            case '>':  r += "&gt;";   break;
            case '"':  r += "&quot;"; break;
            case '\'': r += "&apos;"; break;
            default:   r += c;        break;
        }
    }
    return r;
}

bool SolveSpaceUI::Export3mf(const Platform::Path &filename,
                             const std::vector<ThreeMfObject> &objects) {
    // The colours, as base materials; then each object's vertices (shared) and triangles.
    std::vector<uint32_t> colours;
    auto colourIndex = [&](RgbaColor c) {
        uint32_t packed = c.ToPackedInt();
        for(size_t i = 0; i < colours.size(); i++) {
            if(colours[i] == packed) return (int)i;
        }
        colours.push_back(packed);
        return (int)colours.size() - 1;
    };
    std::string objectsXml, buildXml;
    int         id = 2;
    for(const ThreeMfObject &o : objects) {
        std::map<std::tuple<double, double, double>, int> index;
        std::string vertices, triangles;
        auto vertex = [&](Vector v) {
            auto key = std::make_tuple(v.x, v.y, v.z);
            auto it  = index.find(key);
            if(it != index.end()) return it->second;
            int n = (int)index.size();
            index[key] = n;
            vertices += ssprintf("<vertex x=\"%.6f\" y=\"%.6f\" z=\"%.6f\"/>\n", v.x, v.y, v.z);
            return n;
        };
        for(const STriangle &t : o.mesh->l) {
            int a = vertex(t.a), b = vertex(t.b), c = vertex(t.c);
            if(a == b || b == c || c == a) continue;
            triangles += ssprintf("<triangle v1=\"%d\" v2=\"%d\" v3=\"%d\" pid=\"1\" p1=\"%d\"/>\n",
                                  a, b, c, colourIndex(t.meta.color));
        }
        objectsXml += ssprintf("<object id=\"%d\" type=\"model\" name=\"%s\">\n<mesh>\n"
                               "<vertices>\n%s</vertices>\n<triangles>\n%s</triangles>\n"
                               "</mesh>\n</object>\n",
                               id, XmlEscape(o.name).c_str(), vertices.c_str(), triangles.c_str());
        buildXml += ssprintf("<item objectid=\"%d\"/>\n", id);
        id++;
    }
    if(colours.empty()) colours.push_back(RgbaColor::From(200, 200, 200).ToPackedInt());
    std::string materials = "<basematerials id=\"1\">\n";
    for(size_t i = 0; i < colours.size(); i++) {
        RgbaColor c = RgbaColor::FromPackedInt(colours[i]);
        materials += ssprintf("<base name=\"colour %d\" displaycolor=\"#%02X%02X%02X\"/>\n",
                              (int)i + 1, c.red, c.green, c.blue);
    }
    materials += "</basematerials>\n";

    std::string model =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model unit=\"millimeter\" xml:lang=\"en-US\" "
        "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
        "<metadata name=\"Application\">SolveSpace NG</metadata>\n"
        "<resources>\n" + materials + objectsXml + "</resources>\n"
        "<build>\n" + buildXml + "</build>\n</model>\n";
    std::string types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        "<Default Extension=\"rels\" "
        "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        "<Default Extension=\"model\" "
        "ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
        "</Types>\n";
    std::string rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
        "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
        "</Relationships>\n";

    std::string zip;
    if(!Zip({ { "[Content_Types].xml", types }, { "_rels/.rels", rels },
              { "3D/3dmodel.model", model } },
            &zip)) {
        Error(_("The 3MF file would be over 4 GB; it was not written."));
        return false;
    }
    FILE *f = OpenFile(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.raw.c_str());
        return false;
    }
    bool ok = fwrite(zip.data(), 1, zip.size(), f) == zip.size();
    fclose(f);
    return ok;
}

} // namespace SolveSpace
