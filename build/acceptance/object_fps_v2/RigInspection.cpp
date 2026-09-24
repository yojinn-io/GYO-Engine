#include "RigInspection.hpp"
#include "model/Animation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>

void InspectEnemyRig(const Engine::Model::ModelAsset& model,std::ostream& output) {
    using namespace Engine::Model;
    output<<std::fixed<<std::setprecision(6);
    Pose reference;
    auto made=MakeDefaultPose(model,reference);
    if(!made) throw std::runtime_error(made.error());
    for(std::size_t node=0;node<model.nodes.size();++node) {
        const auto p=TransformPoint(reference.globalTransforms[node],{});
        output<<"node="<<model.nodes[node].name<<" reference="<<p.x<<","<<p.y<<","<<p.z<<"\n";
    }
    if(const auto head=model.FindNode("Head")) {
        const auto& matrix=reference.globalTransforms[*head];
        const auto origin=TransformPoint(matrix,{});
        for(const auto axis:std::array<Vec3,3>{{{1,0,0},{0,1,0},{0,0,1}}}) {
            const auto tip=TransformPoint(matrix,axis);
            output<<"Head_local_axis="<<axis.x<<","<<axis.y<<","<<axis.z<<" model_direction="
                <<tip.x-origin.x<<","<<tip.y-origin.y<<","<<tip.z-origin.z<<"\n";
        }
        Vec3 low{1e9F,1e9F,1e9F},high{-1e9F,-1e9F,-1e9F};
        std::vector<Vec3> headVertices;
        for(std::size_t mesh=0;mesh<model.meshes.size();++mesh) {
            std::vector<SkinnedVertex> skinned;
            auto result=SkinMesh(model,mesh,reference,skinned);
            if(!result) throw std::runtime_error(result.error());
            const auto& source=model.meshes[mesh];
            for(std::size_t vertex=0;vertex<source.vertices.size();++vertex) {
                float headWeight=0;
                for(std::size_t weight=0;weight<4;++weight) {
                    const auto& v=source.vertices[vertex];
                    if(v.weights[weight]>0&&source.joints[v.joints[weight]].nodeIndex==*head) headWeight+=v.weights[weight];
                }
                if(headWeight<0.5F) continue;
                const auto p=skinned[vertex].position;headVertices.push_back(p);
                low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
                high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
            }
        }
        if(!headVertices.empty()) {
            const Vec3 center{(low.x+high.x)*0.5F,(low.y+high.y)*0.5F,(low.z+high.z)*0.5F};
            const Vec3 delta{center.x-origin.x,center.y-origin.y,center.z-origin.z};
            const auto& m=matrix.values;
            const Vec3 x{m[0],m[1],m[2]},y{m[4],m[5],m[6]},z{m[8],m[9],m[10]};
            const auto cross=[](Vec3 a,Vec3 b)->Vec3 {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
            const auto dot=[](Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;};
            const float determinant=dot(x,cross(y,z));
            const Vec3 local{dot(delta,cross(y,z))/determinant,dot(delta,cross(z,x))/determinant,dot(delta,cross(x,y))/determinant};
            float radiusSquared=0;
            for(auto p:headVertices) {p={p.x-center.x,p.y-center.y,p.z-center.z};radiusSquared=std::max(radiusSquared,dot(p,p));}
            output<<"Head_weighted_bounds_min="<<low.x<<","<<low.y<<","<<low.z<<" max="<<high.x<<","<<high.y<<","<<high.z
                <<" count="<<headVertices.size()<<"\nHead_model_center="<<center.x<<","<<center.y<<","<<center.z
                <<" local_center="<<local.x<<","<<local.y<<","<<local.z<<" enclosing_radius="<<std::sqrt(radiusSquared)<<"\n";
        }
    }
    std::vector<SkinnedVertex> vertices;
    float minimum=std::numeric_limits<float>::max(),maximum=-minimum;
    for(std::size_t mesh=0;mesh<model.meshes.size();++mesh) {
        auto skinned=SkinMesh(model,mesh,reference,vertices);
        if(!skinned) throw std::runtime_error(skinned.error());
        for(const auto& vertex:vertices) {minimum=std::min(minimum,vertex.position.y);maximum=std::max(maximum,vertex.position.y);}
    }
    output<<"reference_height="<<maximum-minimum<<" reference_feet="<<minimum<<" scale_to_1.6="<<1.6F/(maximum-minimum)<<"\n";
    for(const char* name:{"Armature|Punch_Jab","Armature|Pistol_Shoot","Armature|Death01"}) {
        const auto clip=model.FindClip(name);
        if(!clip) continue;
        const double duration=model.clips[*clip].durationSeconds;
        output<<"clip="<<name<<" duration="<<duration<<"\n";
        if(std::string_view(name)=="Armature|Death01") continue;
        struct Extremum { float min=std::numeric_limits<float>::max(),max=-min;double minTime{},maxTime{}; };
        std::array<Extremum,2> extrema;
        constexpr std::array hands{"hand_l","hand_r"};
        for(std::size_t frame=0;frame<=static_cast<std::size_t>(duration*240.0+0.5);++frame) {
            const double time=std::min(duration,frame/240.0);
            Pose pose;
            const auto sampled=SamplePose(model,*clip,time,PlaybackMode::Clamp,pose);
            if(!sampled) throw std::runtime_error(sampled.error());
            for(std::size_t h=0;h<hands.size();++h) {
                const auto node=model.FindNode(hands[h]);
                if(!node) throw std::runtime_error(std::string("missing hand ")+hands[h]);
                const auto p=TransformPoint(pose.globalTransforms[*node],{});
                auto& e=extrema[h];
                if(p.z<e.min) {e.min=p.z;e.minTime=time;}
                if(p.z>e.max) {e.max=p.z;e.maxTime=time;}
                if(frame%8==0) {
                    output<<"sample="<<time<<" hand="<<hands[h]<<" position="<<p.x<<","<<p.y<<","<<p.z;
                    const auto& m=pose.globalTransforms[*node].values;
                    output<<" x_axis="<<m[0]<<","<<m[1]<<","<<m[2]
                        <<" y_axis="<<m[4]<<","<<m[5]<<","<<m[6]
                        <<" z_axis="<<m[8]<<","<<m[9]<<","<<m[10]<<"\n";
                }
            }
        }
        for(std::size_t h=0;h<hands.size();++h) {
            const auto& e=extrema[h];
            output<<"hand="<<hands[h]<<" z_min="<<e.min<<" at="<<e.minTime<<" z_max="<<e.max<<" at="<<e.maxTime<<"\n";
        }
    }
}
