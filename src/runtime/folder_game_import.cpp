#include "runtime/folder_game_import.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
namespace playden::runtime { namespace {
bool id_ok(const std::string&s){return !s.empty()&&s.size()<=64&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';});}
bool rel_ok(const std::string&s,bool empty=false){
 if(s.empty())return empty;
 if(s.size()>1024||s.front()=='/'||s.find('\\')!=s.npos||s.find('\0')!=s.npos)return false;
 std::filesystem::path p(s); if(p.is_absolute()||p.has_root_name()||p.lexically_normal().generic_string()!=s)return false;
 for(auto&part:p)if(part=="."||part==".."||part.empty())return false; return true;
}
bool within(const std::filesystem::path&r,const std::filesystem::path&p){auto a=r.begin(),b=p.begin();for(;a!=r.end();++a,++b)if(b==p.end()||*a!=*b)return false;return true;}
std::string esc(const std::string&s){std::string o;for(unsigned char c:s){switch(c){case '"':o+="\\\"";break;case '\\':o+="\\\\";break;case '\n':o+="\\n";break;case '\r':o+="\\r";break;case '\t':o+="\\t";break;default:if(c<32)return {};o+=char(c);}}return o;}
bool field(const std::string&j,const std::string&key,std::size_t from,std::string*out){
 auto p=j.find("\""+key+"\":\"",from);if(p==j.npos)return false;p+=key.size()+4;out->clear();
 while(p<j.size()){char c=j[p++];if(c=='"')return true;if(c=='\\'){if(p==j.size())return false;char e=j[p++];if(e=='n')c='\n';else if(e=='r')c='\r';else if(e=='t')c='\t';else if(e=='"'||e=='\\'||e=='/')c=e;else return false;}else if((unsigned char)c<32)return false;out->push_back(c);}return false;
}
bool valid(const FolderGame&g,std::string*why){
 if(!id_ok(g.id)||g.name.empty()||g.name.size()>160||g.arguments.size()>4096||g.arguments.find('\0')!=g.arguments.npos||!rel_ok(g.executable)||!rel_ok(g.working_directory,true)){if(why)*why="Invalid game ID, name, arguments, or relative path.";return false;}
 std::error_code e;auto r=std::filesystem::canonical(g.root,e);if(e||!std::filesystem::is_directory(r,e)||e){if(why)*why="Game folder does not exist.";return false;}
 auto x=std::filesystem::canonical(r/g.executable,e);if(e||!within(r,x)||!std::filesystem::is_regular_file(x,e)||e){if(why)*why="Executable is missing or resolves outside the game folder.";return false;}
 auto ext=x.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});if(ext!=".exe"&&ext!=".com"&&ext!=".bat"){if(why)*why="Choose a .exe, .com, or .bat file.";return false;}
 if(!g.working_directory.empty()){auto w=std::filesystem::canonical(r/g.working_directory,e);if(e||!within(r,w)||!std::filesystem::is_directory(w,e)||e){if(why)*why="Working directory is missing or outside the game folder.";return false;}}
 return true;
}
}
FolderImportResult ValidateFolderGame(const std::string&id,const std::string&name,const std::filesystem::path&root,const std::string&exe,const std::string&args,const std::string&wd){
 FolderGame g{id,name,root,exe,wd,args};std::string why;if(!valid(g,&why))return {false,why,{}};std::error_code e;g.root=std::filesystem::canonical(root,e);return {true,"Validated; game files were not copied or changed.",g};
}
std::vector<std::string> DiscoverFolderExecutables(const std::filesystem::path&path,std::size_t max){
 std::vector<std::string> out;std::error_code e;auto root=std::filesystem::canonical(path,e);if(e||!std::filesystem::is_directory(root,e)||e||!max)return out;
 std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,e),end;
 for(;!e&&it!=end&&out.size()<max;it.increment(e)){auto s=it->symlink_status(e);if(e){e.clear();continue;}if(std::filesystem::is_symlink(s)){it.disable_recursion_pending();continue;}if(!std::filesystem::is_regular_file(s))continue;auto ext=it->path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});if(ext!=".exe"&&ext!=".com"&&ext!=".bat")continue;auto p=it->path().lexically_relative(root).generic_string();if(rel_ok(p))out.push_back(p);}
 std::sort(out.begin(),out.end());return out;
}
bool LoadFolderGameCatalog(const std::filesystem::path&path,std::vector<FolderGame>*games,std::string*error){
 if(!games){if(error)*error="No output collection supplied.";return false;}std::ifstream f(path,std::ios::binary);if(!f){if(error)*error="Could not open imported-game catalog.";return false;}
 std::string j((std::istreambuf_iterator<char>(f)),{});if(j.size()>1024*1024||j.find("\"schema_version\":1")==j.npos){if(error)*error="Catalog too large or unsupported schema.";return false;}
 auto p=j.find("\"games\":[");if(p==j.npos){if(error)*error="Catalog has no games list.";return false;}p=j.find('[',p)+1;std::vector<FolderGame> result;std::set<std::string> ids;
 while(p<j.size()){auto a=j.find('{',p),b=j.find('}',p);if(b==j.npos||a==j.npos||b<a)break;FolderGame g;std::string root;
 if(!field(j,"id",a,&g.id)||!field(j,"name",a,&g.name)||!field(j,"root",a,&root)||!field(j,"executable",a,&g.executable)||!field(j,"working_directory",a,&g.working_directory)||!field(j,"arguments",a,&g.arguments)){if(error)*error="Malformed catalog entry.";return false;}
 g.root=root;if(!ids.insert(g.id).second||!valid(g,error))return false;result.push_back(g);p=b+1;
 }*games=std::move(result);return true;
}
bool SaveFolderGameCatalog(const std::filesystem::path&path,const std::vector<FolderGame>&games,std::string*error){
 if(games.size()>128){if(error)*error="Catalog limit is 128 games.";return false;}std::set<std::string>ids;std::ostringstream o;o<<"{\"schema_version\":1,\"games\":[";bool first=true;
 for(auto&g:games){if(!ids.insert(g.id).second||!valid(g,error))return false;std::error_code e;auto root=std::filesystem::canonical(g.root,e);if(e)return false;if(!first)o<<',';first=false;o<<"{\"id\":\""<<esc(g.id)<<"\",\"name\":\""<<esc(g.name)<<"\",\"root\":\""<<esc(root.string())<<"\",\"executable\":\""<<esc(g.executable)<<"\",\"working_directory\":\""<<esc(g.working_directory)<<"\",\"arguments\":\""<<esc(g.arguments)<<"\"}";}
 o<<"]}";std::error_code e;std::filesystem::create_directories(path.parent_path(),e);if(e){if(error)*error="Could not create catalog directory.";return false;}auto tmp=path.string()+".tmp";{std::ofstream f(tmp,std::ios::binary|std::ios::trunc);if(!f||!(f<<o.str())||!f.flush()){if(error)*error="Could not write catalog.";return false;}}
 std::filesystem::rename(tmp,path,e);if(e){std::filesystem::remove(tmp);if(error)*error="Could not publish catalog atomically.";return false;}return true;
}
}
