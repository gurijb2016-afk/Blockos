#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

// Fájltípusok definíciói a Blockos VFS-hez
enum class TmpNodeType {
    Regular,
    Directory
};

// Egy node (fájl vagy mappa) reprezentációja a RAM-ban
struct TmpFSNode {
    std::string name;
    TmpNodeType type;
    uint32_t size;
    uint32_t permissions;
    
    // Ha REGULAR fájl: itt tároljuk a nyers bájtokat a RAM-ban
    std::vector<uint8_t> data;
    
    // Ha DIRECTORY: a mappában lévő gyermek-node-ok listája (Név -> Node)
    std::map<std::string, std::shared_ptr<TmpFSNode>> children;

    TmpFSNode(std::string_view node_name, TmpNodeType node_type)
        : name(node_name), type(node_type), size(0), permissions(0644) {}
};

class TmpFileSystem {
private:
    std::shared_ptr<TmpFSNode> m_root;

    // Segédfüggvény egy elérési út (path) feloldásához
    std::shared_ptr<TmpFSNode> find_node(const std::string& path);
    std::vector<std::string> split_path(const std::string& path);

public:
    TmpFileSystem();
    ~TmpFileSystem() = default;

    // Core VFS API műveletek
    bool create_file(const std::string& path, uint32_t mode = 0644);
    bool mkdir(const std::string& path, uint32_t mode = 0755);
    
    int32_t write(const std::string& path, const uint8_t* buffer, uint32_t size, uint32_t offset);
    int32_t read(const std::string& path, uint8_t* buffer, uint32_t size, uint32_t offset);
    
    bool remove(const std::string& path);
    std::vector<std::string> list_directory(const std::string& path);
};
