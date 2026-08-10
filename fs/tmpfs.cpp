#include "tmpfs.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>

TmpFileSystem::TmpFileSystem() {
    // A gyökér (root) mappa inicializálása a RAM-ban
    m_root = std::make_shared<TmpFSNode>("", TmpNodeType::Directory);
    m_root->permissions = 0755;
}

// Elérési út felosztása '/' karakterek mentén
std::vector<std::string> TmpFileSystem::split_path(const std::string& path) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(path);
    while (std::getline(tokenStream, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// Belső fa-bejáró logikája a kereséshez
std::shared_ptr<TmpFSNode> TmpFileSystem::find_node(const std::string& path) {
    if (path == "/" || path.empty()) {
        return m_root;
    }

    auto tokens = split_path(path);
    auto current = m_root;

    for (const auto& token : tokens) {
        if (current->type != TmpNodeType::Directory) {
            return nullptr; // Nem mappa, nem lehet benne tovább keresni
        }

        auto it = current->children.find(token);
        if (it == current->children.end()) {
            return nullptr; // A keresett komponens nem létezik
        }
        current = it->second;
    }
    return current;
}

// Új üres fájl létrehozása a memóriában
bool TmpFileSystem::create_file(const std::string& path, uint32_t mode) {
    auto tokens = split_path(path);
    if (tokens.empty()) return false;

    std::string file_name = tokens.back();
    tokens.pop_back();

    // Szülő mappa megkeresése
    std::string parent_path = "";
    for (const auto& t : tokens) parent_path += "/" + t;
    if (parent_path.empty()) parent_path = "/";

    auto parent_node = find_node(parent_path);
    if (!parent_node || parent_node->type != TmpNodeType::Directory) {
        return false;
    }

    // Ellenőrizzük, létezik-e már
    if (parent_node->children.find(file_name) != parent_node->children.end()) {
        return false; 
    }

    auto new_file = std::make_shared<TmpFSNode>(file_name, TmpNodeType::Regular);
    new_file->permissions = mode;
    parent_node->children[file_name] = new_file;
    return true;
}

// Új mappa létrehozása a memóriában
bool TmpFileSystem::mkdir(const std::string& path, uint32_t mode) {
    auto tokens = split_path(path);
    if (tokens.empty()) return false;

    std::string dir_name = tokens.back();
    tokens.pop_back();

    std::string parent_path = "";
    for (const auto& t : tokens) parent_path += "/" + t;
    if (parent_path.empty()) parent_path = "/";

    auto parent_node = find_node(parent_path);
    if (!parent_node || parent_node->type != TmpNodeType::Directory) {
        return false;
    }

    if (parent_node->children.find(dir_name) != parent_node->children.end()) {
        return false;
    }

    auto new_dir = std::make_shared<TmpFSNode>(dir_name, TmpNodeType::Directory);
    new_dir->permissions = mode;
    parent_node->children[dir_name] = new_dir;
    return true;
}

// Írás a memóriapufferbe (dinamikus átméretezéssel)
int32_t TmpFileSystem::write(const std::string& path, const uint8_t* buffer, uint32_t size, uint32_t offset) {
    auto node = find_node(path);
    if (!node || node->type != TmpNodeType::Regular) {
        return -1; // Hiba: nem létezik vagy mappa
    }

    // Ha túlmutat az offset + méret az aktuális vektoron, megnöveljük a RAM-puffert
    if (offset + size > node->data.size()) {
        node->data.resize(offset + size);
    }

    std::memcpy(node->data.data() + offset, buffer, size);
    node->size = node->data.size();
    return size;
}

// Olvasás a memóriapufferből
int32_t TmpFileSystem::read(const std::string& path, uint8_t* buffer, uint32_t size, uint32_t offset) {
    auto node = find_node(path);
    if (!node || node->type != TmpNodeType::Regular) {
        return -1;
    }

    if (offset >= node->size) {
        return 0; // EOF (Fájl vége)
    }

    // Ne olvassunk túl a fájl tényleges végén
    uint32_t bytes_to_read = std::min(size, node->size - offset);
    std::memcpy(buffer, node->data.data() + offset, bytes_to_read);
    return bytes_to_read;
}

// Mappa tartalmának kilistázása
std::vector<std::string> TmpFileSystem::list_directory(const std::string& path) {
    std::vector<std::string> result;
    auto node = find_node(path);
    if (!node || node->type != TmpNodeType::Directory) {
        return result;
    }

    for (const auto& [name, child] : node->children) {
        result.push_back(name);
    }
    return result;
}

// Törlés (fájl vagy üres mappa)
bool TmpFileSystem::remove(const std::string& path) {
    auto tokens = split_path(path);
    if (tokens.empty()) return false;

    std::string target_name = tokens.back();
    tokens.pop_back();

    std::string parent_path = "";
    for (const auto& t : tokens) parent_path += "/" + t;
    if (parent_path.empty()) parent_path = "/";

    auto parent_node = find_node(parent_path);
    if (!parent_node) return false;

    auto it = parent_node->children.find(target_name);
    if (it == parent_node->children.end()) return false;

    // Ha mappa, csak akkor törölhető, ha üres
    if (it->second->type == TmpNodeType::Directory && !it->second->children.empty()) {
        return false; 
    }

    parent_node->children.erase(it);
    return true;
}
