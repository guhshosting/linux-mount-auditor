#include "mount_auditor/api.hpp"
#include <algorithm>
#include <charconv>

namespace lma { namespace {
std::vector<std::string> options(std::string_view s){std::vector<std::string>o;std::size_t p=0;while(p<=s.size()){auto e=s.find(',',p);if(e==std::string_view::npos)e=s.size();o.emplace_back(s.substr(p,e-p));if(e==s.size())break;p=e+1;}return o;}
bool has(const std::vector<std::string>&v,std::string_view x){return std::find(v.begin(),v.end(),x)!=v.end();}
bool integer(std::string_view s,int&v){auto r=std::from_chars(s.data(),s.data()+s.size(),v);return !s.empty()&&r.ec==std::errc{}&&r.ptr==s.data()+s.size()&&v>=0;}
template<class T> void append_number(std::string& output,T value){char buffer[32];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);if(result.ec==std::errc{})output.append(buffer,result.ptr);}
std::string unescape(std::string_view s,bool&ok){ok=true;std::string o;for(std::size_t i=0;i<s.size();++i){if(s[i]!='\\'){o+=s[i];continue;}if(i+3>=s.size()){ok=false;o+='\\';continue;}auto q=s.substr(i+1,3);if(q=="040"){o+=' ';i+=3;}else if(q=="011"){o+='\t';i+=3;}else if(q=="012"){o+='\n';i+=3;}else if(q=="134"){o+='\\';i+=3;}else{ok=false;o+='\\';}}return o;}
std::vector<std::string> fields(std::string_view line,bool&ok){std::vector<std::string>v;ok=true;std::string cur;bool escaped=false;for(std::size_t i=0;i<line.size();++i){char c=line[i];if(!escaped&&c=='#')break;if(!escaped&&(c==' '||c=='\t')){if(!cur.empty()){bool q=true;v.push_back(unescape(cur,q));ok=ok&&q;cur.clear();}continue;}cur+=c;if(escaped)escaped=false;else if(c=='\\')escaped=true;}if(!cur.empty()){bool q=true;v.push_back(unescape(cur,q));ok=ok&&q;}return v;}
}
ParseResult<FstabEntry> parse_fstab(std::string_view input){
 ParseResult<FstabEntry>r;if(input.size()>kMaxInput){r.complete=false;r.diagnostics.push_back({"error","fstab","input exceeds 16 MiB",0});return r;}std::size_t p=0,line=0;
 while(p<input.size()){
  auto e=input.find('\n',p);if(e==std::string_view::npos)e=input.size();auto t=input.substr(p,e-p);p=e<input.size()?e+1:e;++line;
  if(t.size()>kMaxLine){r.complete=false;r.diagnostics.push_back({"error","fstab","line exceeds 1 MiB",line});continue;}
  bool ok=true;auto f=fields(t,ok);if(f.empty())continue;if(!ok)r.diagnostics.push_back({"warning","fstab","malformed or unknown escape preserved",line});
  if(f.size()!=6){r.complete=false;std::string message="expected exactly six fields, found ";append_number(message,f.size());r.diagnostics.push_back({"error","fstab",std::move(message),line});continue;}
  FstabEntry x;x.spec=f[0];x.target=f[1];x.fs_type=f[2];x.options=options(f[3]);x.line=line;
  if(!integer(f[4],x.dump)||!integer(f[5],x.pass)){r.complete=false;r.diagnostics.push_back({"error","fstab","invalid dump or pass number",line});continue;}
  x.noauto=has(x.options,"noauto");x.nofail=has(x.options,"nofail");x.bind=has(x.options,"bind")||has(x.options,"rbind");x.swap=x.fs_type=="swap";r.records.push_back(std::move(x));
  if(r.records.size()>=kMaxRecords){r.complete=false;r.diagnostics.push_back({"error","fstab","record limit exceeded",line});break;}
 }
 return r;
}
}
