#include "mount_auditor/api.hpp"
#include <iostream>
#include <sstream>
#include <string>

namespace { int checks=0, failures=0; void check(bool ok,const char*name){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<name<<'\n';}} }
#define CHECK(x) check((x),#x)
int main(){
 bool ok=false;CHECK(lma::decode_mountinfo("a\\040b\\011c\\012d\\134e",ok)==std::string("a b\tc\nd\\e"));CHECK(ok);CHECK(lma::decode_mountinfo("x\\999",ok)=="x\\999");CHECK(!ok);
 const std::string mi=
 "24 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw,errors=remount-ro\n"
 "25 24 0:5 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw\n"
 "26 24 0:42 / /run rw,nosuid,nodev - tmpfs tmpfs rw,size=1m\n"
 "27 24 8:2 / /home ro,noexec master:1 propagate_from:2 - ext4 /dev/sda2 ro\n"
 "28 24 0:55 / /x rw shared:7 master:2 - overlay overlay rw,lowerdir=/a\n"
 "29 24 8:1 /sub /bind rw unbindable unknown:x - ext4 /dev/sda1 rw\n"
 "30 24 0:57 / /nfs rw - nfs4 server:/share rw\n"
 "31 24 0:58 / /cifs rw - cifs //server/share rw\n"
 "32 24 0:59 / /fuse rw - fuse.sshfs host:/ rw\n"
 "33 24 0:60 / /cg rw - cgroup2 cgroup rw\n";
 auto m=lma::parse_mountinfo(mi);CHECK(m.complete);CHECK(m.records.size()==10);CHECK(m.records[0].shared==1);CHECK(m.records[0].propagation=="shared");CHECK(m.records[1].classification=="pseudo/virtual kernel filesystem");CHECK(m.records[2].classification=="tmpfs/ram-backed");CHECK(m.records[3].read_only);CHECK(m.records[3].propagation=="slave");CHECK(m.records[4].propagation=="shared-and-slave");CHECK(m.records[4].classification=="overlay/union filesystem");CHECK(m.records[5].unbindable);CHECK(m.records[5].bind_like);CHECK(m.records[6].classification=="network filesystem");CHECK(m.records[7].classification=="network filesystem");CHECK(m.records[8].classification=="FUSE filesystem");CHECK(m.records[9].classification=="pseudo/virtual kernel filesystem");
 auto bad=lma::parse_mountinfo("bad\n1 2 8:x / / rw - ext4 /dev/x rw\n1 2 8:1 / / rw ext4 /dev/x rw\n");CHECK(!bad.complete);CHECK(bad.records.empty());CHECK(bad.diagnostics.size()==3);
 auto dup=lma::parse_mountinfo("1 9 8:1 / / rw - ext4 /dev/a rw\n1 9 8:1 / /x rw - ext4 /dev/a rw\n");CHECK(!dup.complete);CHECK(dup.records.size()==1);
 auto missing=lma::build_hierarchy(dup.records);CHECK(missing.roots.size()==1);CHECK(!missing.diagnostics.empty());
 auto cyc=lma::parse_mountinfo("1 2 8:1 / /a rw - ext4 /dev/a rw\n2 1 8:1 / /b rw - ext4 /dev/a rw\n");auto ch=lma::build_hierarchy(cyc.records);CHECK(!ch.diagnostics.empty());
 const std::string fs="# c\n/dev/sda1 / ext4 defaults 0 1\nLABEL=My\\040Disk /media/My\\040Disk ext4 noauto,nofail,x-systemd.automount 0 2\n/dev/a /bind none bind 0 0\nserver:/x /net nfs _netdev 0 0\n/swap none swap sw 0 0\n";
 auto f=lma::parse_fstab(fs);CHECK(f.complete);CHECK(f.records.size()==5);CHECK(f.records[1].spec=="LABEL=My Disk");CHECK(f.records[1].target=="/media/My Disk");CHECK(f.records[1].noauto);CHECK(f.records[1].nofail);CHECK(f.records[2].bind);CHECK(f.records[4].swap);
 auto fb=lma::parse_fstab("/dev/a / ext4 defaults 0\n/dev/a / ext4 defaults x 1\n/dev/a / ext4 defaults 0 1 extra\n");CHECK(!fb.complete);CHECK(fb.records.empty());CHECK(fb.diagnostics.size()==3);
 const std::string escaped="#x\n/dev/a /a\\040b ext4 defaults 0 0 # trailing\n/dev/b /t\\134x ext4 defaults 0 0\n";auto fe=lma::parse_fstab(escaped);CHECK(fe.records.size()==2);CHECK(fe.records[0].target=="/a b");CHECK(fe.records[1].target=="/t\\x");
 std::vector<lma::FstabEntry> cf={{"/dev/sda1","/","ext4",{"rw"},std::nullopt,0,1,1,false,false,false,false},{"/dev/sda2","/home","ext4",{"ro"},std::nullopt,0,2,2,false,false,false,false},{"/dev/no","/absent","ext4",{"noauto"},std::nullopt,0,0,3,true,false,false,false}};
 auto comp=lma::compare(m.records,cf,true);CHECK(comp.size()>=3);CHECK(comp[0].status=="represented");CHECK(comp[1].status=="represented");CHECK(comp[2].status=="not-expected-automatically");
 lma::FstabEntry bind_entry{"/dev/sda1","/bind","none",{"bind"},std::nullopt,0,0,4,false,false,true,false};auto bind_cmp=lma::compare({m.records[5]},{bind_entry},true);CHECK(!bind_cmp.empty()&&bind_cmp[0].status=="represented");
 std::vector<lma::Diagnostic> none;auto findings=lma::audit(m.records,cf,comp,none);bool broken=false;for(const auto&x:findings)if(x.severity=="error"||x.category=="broken")broken=true;CHECK(!broken);
 CHECK(lma::path_beneath("/a/b","/a"));CHECK(!lma::path_beneath("/ab","/a"));CHECK(lma::path_beneath("/x","/"));CHECK(lma::json_escape("\"\\\n\t")=="\\\"\\\\\\n\\t");
 std::ostringstream jo;lma::render_mounts_json(jo,m.records,none);CHECK(jo.str().find("\"schema_version\":1")!=std::string::npos);CHECK(jo.str().find("\"current_mounts\"")!=std::string::npos);CHECK(jo.str().back()=='\n');
 std::ostringstream fj;lma::render_fstab_json(fj,f.records,none);CHECK(fj.str().find("My Disk")!=std::string::npos);std::ostringstream cj;lma::render_compare_json(cj,comp,none);CHECK(cj.str().find("represented")!=std::string::npos);std::ostringstream aj;lma::render_audit_json(aj,findings,none);CHECK(aj.str().find("findings")!=std::string::npos);
 std::string deep;for(int i=1;i<=5000;++i)deep+=std::to_string(i)+" "+std::to_string(i==1?999999:i-1)+" 8:1 / /d"+std::to_string(i)+" rw - ext4 /dev/a rw\n";auto dm=lma::parse_mountinfo(deep);CHECK(dm.records.size()==5000);auto dh=lma::build_hierarchy(dm.records);CHECK(!dh.roots.empty());
 std::string huge(lma::kMaxInput+1,'x');CHECK(!lma::parse_mountinfo(huge).complete);CHECK(!lma::parse_fstab(huge).complete);
 std::string bounded_error;CHECK(!lma::read_bounded_file("/proc/self/mountinfo",1,bounded_error)&&bounded_error.find("input exceeds limit")!=std::string::npos);
 // Mandatory false-positive acceptance set: unusual classes and context are never emitted as errors.
 const char* safe[]={"overlay/union filesystem","network filesystem","FUSE filesystem","tmpfs/ram-backed","bind/subtree mount"};for(const char*s:safe){lma::Mount x;x.target=s;x.classification=s;auto a=lma::audit({x},{},{},{});bool e=false;for(const auto&q:a)e|=q.severity=="error";CHECK(!e);}
 std::vector<lma::Mount> many(1000);for(std::size_t i=0;i<many.size();++i){many[i].id=i+1;many[i].target="/container/"+std::to_string(i);many[i].source="overlay";many[i].classification="overlay/union filesystem";}CHECK(lma::audit(many,{},{},{}).empty());
 auto dynamic_cmp=lma::compare({m.records[0]}, {}, true);CHECK(dynamic_cmp.size()==1&&dynamic_cmp[0].status=="current-without-obvious-fstab");
 lma::FstabEntry noauto{"/dev/x","/not-mounted","ext4",{"noauto"},std::nullopt,0,0,1,true,false,false,false};auto noauto_cmp=lma::compare({}, {noauto}, true);CHECK(noauto_cmp.size()==1&&noauto_cmp[0].status=="not-expected-automatically");
 lma::Mount same1=m.records[0],same2=m.records[0];same2.id=999;same2.target="/second";auto same_find=lma::audit({same1,same2},{},{},{});CHECK(same_find.size()==2&&same_find[0].severity=="info"&&same_find[1].severity=="info");
 lma::Mount shared=m.records[0];shared.propagation="shared";auto shared_find=lma::audit({shared},{},{},{});CHECK(shared_find.size()==1&&shared_find[0].severity=="info");
 auto ro_cmp=lma::compare({m.records[3]}, {cf[1]}, true);CHECK(!ro_cmp.empty()&&ro_cmp[0].status=="represented");
 lma::Mount ns;ns.id=1;ns.parent_id=999;ns.target="/vpn";auto nsh=lma::build_hierarchy({ns});CHECK(nsh.diagnostics.size()==1&&nsh.diagnostics[0].level=="info");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
