#pragma once
#ifdef _WIN32
#include <windows.h>
#include <string>
#include <vector>
namespace krmemory {
struct RelationGraphNode {std::wstring signature,label,kind;};
struct RelationGraphEdge {std::wstring from,to,type;};
HWND create_relation_graph(HWND parent,int control_id);
void set_relation_graph_data(HWND graph,std::vector<RelationGraphNode> nodes,std::vector<RelationGraphEdge> edges);
void center_relation_graph(HWND graph);
}
#endif
