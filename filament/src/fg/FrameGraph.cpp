/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FrameGraph.h"

#include "FrameGraphPassResources.h"

#include "driver/Driver.h"
#include "driver/Handle.h"
#include "driver/CommandStream.h"

#include <utils/Panic.h>
#include <utils/Log.h>

using namespace utils;

namespace filament {

using namespace fg;

// ------------------------------------------------------------------------------------------------

namespace fg {

struct Resource {
    explicit Resource(const char* name) noexcept : name(name) { }

    ~Resource() noexcept;

    // constants
    const char* const name;         // for debugging

    // computed during compile()
    PassNode* writer = nullptr;     // last writer to this resource
    PassNode* first = nullptr;      // pass that needs to instantiate the resource
    PassNode* last = nullptr;       // pass that can destroy the resource
    uint32_t writerCount = 0;       // # of passes writing to this resource
    uint32_t readerCount = 0;       // # of passes reading from this resource

    // concrete resource
    void create(driver::DriverApi& driver) noexcept;
    void destroy(driver::DriverApi& driver) noexcept;
    FrameGraphResource::Descriptor desc;
    FrameGraph::Builder::RWFlags readFlags = 0;
    FrameGraph::Builder::RWFlags writeFlags = 0;
    Handle<HwTexture> textures[2];  // color, depth
    Handle<HwRenderTarget> target;
};

Resource::~Resource() noexcept {
    assert(!textures[0]);
    assert(!textures[1]);
    assert(!target);
}

void Resource::create(driver::DriverApi& driver) noexcept {
    if (readerCount) {
        assert(readFlags);
        if (readFlags & FrameGraph::Builder::COLOR) {
            textures[0] = driver.createTexture(desc.type, desc.levels,
                    desc.format, 1,
                    desc.width, desc.height, desc.depth,
                    driver::TextureUsage::COLOR_ATTACHMENT);
        }
        if (readFlags & FrameGraph::Builder::DEPTH) {
            textures[1] = driver.createTexture(desc.type, desc.levels,
                    driver::TextureFormat::DEPTH24, 1,
                    desc.width, desc.height, desc.depth,
                    driver::TextureUsage::DEPTH_ATTACHMENT);
        }
    }
    if (writerCount) {
        assert(writeFlags);
        uint32_t attachments = 0;
        if (writeFlags & FrameGraph::Builder::COLOR) {
            attachments |= uint32_t(driver::TargetBufferFlags::COLOR);
        }
        if (writeFlags & FrameGraph::Builder::DEPTH) {
            attachments |= driver::TargetBufferFlags::DEPTH;
        }
        target = driver.createRenderTarget(driver::TargetBufferFlags(attachments),
                desc.width, desc.height, desc.samples, desc.format,
                { textures[0] }, { textures[1] }, {});
    }
}

void Resource::destroy(driver::DriverApi& driver) noexcept {
    for (auto& texture : textures) {
        if (texture) {
            driver.destroyTexture(texture);
        }
    }
    if (target) {
        driver.destroyRenderTarget(target);
    }
}

struct ResourceNode {
    ResourceNode(const char* name, uint16_t index) noexcept : name(name), index(index) { }
    ResourceNode(ResourceNode const&) = delete;
    ResourceNode(ResourceNode&&) noexcept = default;

    // constants
    const char* const name;
    uint16_t const index;

    // updated by the builder
    uint16_t version = 0;
    FrameGraphResource::Descriptor desc;
    FrameGraph::Builder::RWFlags readFlags = 0;
    FrameGraph::Builder::RWFlags writeFlags= 0;

    // set during compile()
    Resource* resource = nullptr;
};

struct PassNode {
    // TODO: use something less heavy than a std::vector<>
    using ResourceList = std::vector<FrameGraphResource>;

    PassNode(const char* name, uint32_t id, FrameGraphPassExecutor* base) noexcept
            : name(name), id(id), base(base) {
    }
    PassNode(PassNode const&) = delete;
    PassNode(PassNode&& rhs) noexcept
            : name(rhs.name), id(rhs.id), base(rhs.base),
              reads(std::move(rhs.reads)),
              writes(std::move(rhs.writes)),
              devirtualize(std::move(rhs.devirtualize)),
              destroy(std::move(rhs.destroy)),
              refCount(rhs.refCount) {
        rhs.base = nullptr;
    }

    PassNode& operator=(PassNode const&) = delete;
    PassNode& operator=(PassNode&&) = delete;

    ~PassNode() { delete base; }

    // for Builder
    void read(ResourceNode const& resource) {
        // just record that we're reading from this resource (at the given version)
        reads.push_back({ resource.index, resource.version });
    }

    void write(ResourceNode& resource) {
        // invalidate existing handles to this resource
        ++resource.version;
        // record the write
        writes.push_back({ resource.index, resource.version });
    }

    // constants
    const char* const name;                     // our name
    const uint32_t id;                          // a unique id (only for debugging)
    FrameGraphPassExecutor* base = nullptr;     // type eraser for calling execute()

    // set by the builder
    ResourceList reads;                     // resources we're reading from
    ResourceList writes;                    // resources we're writing to

    // computed during compile()
    std::vector<uint16_t> devirtualize;     // resources we need to create before executing
    std::vector<uint16_t> destroy;          // resources we need to destroy after executing
    uint32_t refCount = 0;                  // count resources that have a reference to us
};

struct Alias {
    FrameGraphResource from, to;
};

} // namespace fg

// ------------------------------------------------------------------------------------------------

FrameGraphPassExecutor::FrameGraphPassExecutor() = default;
FrameGraphPassExecutor::~FrameGraphPassExecutor() = default;

// ------------------------------------------------------------------------------------------------

FrameGraph::Builder::Builder(FrameGraph& fg, PassNode& pass) noexcept
    : mFrameGraph(fg), mPass(pass) {
}

FrameGraphResource FrameGraph::Builder::createTexture(
        const char* name, FrameGraphResource::Descriptor const& desc) noexcept {
    FrameGraph& frameGraph = mFrameGraph;
    ResourceNode& resource = frameGraph.createResource(name);
    resource.desc = desc;
    return { resource.index, resource.version };
}

FrameGraphResource FrameGraph::Builder::read(FrameGraphResource const& input, RWFlags readFlags) {
    ResourceNode* resource = mFrameGraph.getResource(input);
    if (!resource) {
        return {};
    }
    resource->readFlags |= readFlags;
    mPass.read(*resource);
    return input;
}

FrameGraphResource FrameGraph::Builder::write(FrameGraphResource const& output, RWFlags writeFlags) {
    ResourceNode* resource = mFrameGraph.getResource(output);
    if (!resource) {
        return {};
    }

    resource->writeFlags |= writeFlags;
    mPass.write(*resource);

    /*
     * We invalidate and rename handles that are writen into, to avoid undefined order
     * access to the resources.
     *
     * e.g. forbidden graphs
     *
     *         +-> [R1] -+
     *        /           \
     *  (A) -+             +-> (A)
     *        \           /
     *         +-> [R2] -+        // failure when setting R2 from (A)
     *
     */

    return { resource->index, resource->version };
}

// ------------------------------------------------------------------------------------------------

FrameGraphPassResources::FrameGraphPassResources(FrameGraph& fg, fg::PassNode const& pass) noexcept
    : mFrameGraph(fg), mPass(pass) {
}

Handle<HwTexture> FrameGraphPassResources::getTexture(
        FrameGraphResource r, driver::TextureUsage attachment) const {
    // TODO: we should check that this FrameGraphResource is indeed used by this pass
    (void)mPass; // suppress unused warning
    Resource const* const pResource = mFrameGraph.mResourceNodes[r.index].resource;
    assert(pResource);

    switch (attachment) {
        case driver::TextureUsage::DEFAULT:
            if (pResource->readFlags == FrameGraph::Builder::DEPTH) {
                return pResource->textures[1];
            }
            [[clang::fallthrough]];
        case driver::TextureUsage::COLOR_ATTACHMENT:
            return pResource->textures[0];

        case driver::TextureUsage::DEPTH_ATTACHMENT:
            return pResource->textures[1];
    }
}

Handle<HwRenderTarget> FrameGraphPassResources::getRenderTarget(FrameGraphResource r) const {
    // TODO: we should check that this FrameGraphResource is indeed used by this pass
    (void)mPass; // suppress unused warning
    Resource const* const pResource = mFrameGraph.mResourceNodes[r.index].resource;
    assert(pResource);
    return pResource->target;
}

// ------------------------------------------------------------------------------------------------

FrameGraph::FrameGraph() = default;

FrameGraph::~FrameGraph() = default;

bool FrameGraph::isValid(FrameGraphResource handle) const noexcept {
    if (!handle.isValid()) return false;
    auto const& registry = mResourceNodes;
    assert(handle.index < registry.size());
    auto& resource = registry[handle.index];
    return handle.version == resource.version;
}

void FrameGraph::moveResource(FrameGraphResource from, FrameGraphResource to) {
    // FIXME: what checks need to happen?
    mAliases.push_back({from, to});
}

void FrameGraph::present(FrameGraphResource input) {
    struct Dummy {
    };
    addPass<Dummy>("Present",
            [&input](Builder& builder, Dummy& data) {
                builder.read(input);
            },
            [](FrameGraphPassResources const& resources, Dummy const& data, driver::DriverApi&) {
            });
}

PassNode& FrameGraph::createPass(const char* name, FrameGraphPassExecutor* base) noexcept {
    auto& frameGraphPasses = mPassNodes;
    const uint32_t id = (uint32_t)frameGraphPasses.size();
    frameGraphPasses.emplace_back(name, id, base);
    return frameGraphPasses.back();
}

ResourceNode& FrameGraph::createResource(const char* name) noexcept {
    auto& registry = mResourceNodes;
    uint16_t id = (uint16_t)registry.size();
    registry.emplace_back(name, id);
    return registry.back();
}


ResourceNode* FrameGraph::getResource(FrameGraphResource r) {
    auto& registry = mResourceNodes;

    if (!ASSERT_POSTCONDITION_NON_FATAL(r.isValid(),
            "using an uninitialized resource handle")) {
        return nullptr;
    }

    assert(r.index < registry.size());
    auto& resource = registry[r.index];

    if (!ASSERT_POSTCONDITION_NON_FATAL(r.version == resource.version,
            "using an invalid resource handle (version=%u) for resource=\"%s\" (id=%u, version=%u)",
            r.version, resource.name, resource.index, resource.version)) {
        return nullptr;
    }

    return &resource;
}
FrameGraphResource::Descriptor* FrameGraph::getTextureDesc(FrameGraphResource r) {
    ResourceNode* node = getResource(r);
    return node ? &node->desc : nullptr;
}

FrameGraph& FrameGraph::compile() noexcept {
    auto& resourceNodes = mResourceNodes;
    auto& resourceRegistry = mResourceRegistry;
    resourceRegistry.reserve(resourceNodes.size());

    // create the sub-resources
    for (ResourceNode& node : resourceNodes) {
        resourceRegistry.emplace_back(node.name);
        node.resource = &resourceRegistry.back();
        node.resource->desc = node.desc;
        node.resource->readFlags = node.readFlags;
        node.resource->writeFlags = node.writeFlags;
    }

    // remap them
    for (auto const& alias : mAliases) {
        resourceNodes[alias.to.index].resource = resourceNodes[alias.from.index].resource;
    }

    for (PassNode& pass : mPassNodes) {
        // compute passes reference counts (i.e. resources we're writing to)
        pass.refCount = (uint32_t)pass.writes.size();

        // compute resources reference counts (i.e. resources we're reading from), and first/last users
        for (FrameGraphResource resource : pass.reads) {
            Resource* subResource = resourceNodes[resource.index].resource;

            // add a reference for each pass that reads from this resource
            subResource->readerCount++;

            // figure out which is the first pass to need this resource
            subResource->first = subResource->first ? subResource->first : &pass;

            // figure out which is the last pass to need this resource
            subResource->last = &pass;
        }

        // set the writers
        for (FrameGraphResource resource : pass.writes) {
            Resource* subResource = resourceNodes[resource.index].resource;
            subResource->writer = &pass;

            // add a reference for each pass that writes to this resource
            subResource->writerCount++;

            // figure out which is the first pass to need this resource
            subResource->first = subResource->first ? subResource->first : &pass;

            // figure out which is the last pass to need this resource
            subResource->last = &pass;
        }
    }

    // cull passes and resources...
    std::vector<Resource*> stack;
    stack.reserve(resourceNodes.size());
    for (Resource& resource : resourceRegistry) {
        if (resource.readerCount == 0) {
            stack.push_back(&resource);
        }
    }

    while (!stack.empty()) {
        Resource const* const pSubResource = stack.back();
        stack.pop_back();

        // by construction, this resource cannot have more than one producer because
        // - two unrelated passes can't write in the same resource
        // - passes that read + write into the resource imply that the refcount is not null

        assert(pSubResource->writerCount <= 1);

        // if a resource doesn't have a writer and is not imported, then we the graph is not
        // set correctly. For now we ignore this.
        if (!pSubResource->writer) {
            slog.d << "resource \"" << pSubResource->name << "\" is never written!" << io::endl;
            continue; // TODO: this shouldn't happen in a valid graph
        }

        PassNode* const writer = pSubResource->writer;
        assert(writer->refCount >= 1);
        if (--writer->refCount == 0) {
            // this pass is culled
            auto const& reads = writer->reads;
            for (FrameGraphResource resource : reads) {
                Resource* r = resourceNodes[resource.index].resource;
                if (--r->readerCount == 0) {
                    stack.push_back(r);
                }
            }
        }
    }

    for (size_t index = 0, c = resourceRegistry.size() ; index < c ; index++) {
        auto& resource = resourceRegistry[index];
        assert(!resource.first == !resource.last);
        if (resource.readerCount && resource.first && resource.last) {
            resource.first->devirtualize.push_back((uint16_t)index);
            resource.last->destroy.push_back((uint16_t)index);
        }
    }

    return *this;
}

void FrameGraph::execute(driver::DriverApi& driver) noexcept {
    auto& resourceRegistry = mResourceRegistry;
    for (PassNode const& node : mPassNodes) {
        if (!node.refCount) continue;
        assert(node.base);

        FrameGraphPassResources resources(*this, node);

        // create concrete resources
        for (size_t id : node.devirtualize) {
            resourceRegistry[id].create(driver);
        }

        // execute the pass
        node.base->execute(resources, driver);

        // destroy concrete resources
        for (uint32_t id : node.destroy) {
            resourceRegistry[id].destroy(driver);
        }
    }

    // reset the frame graph state
    mPassNodes.clear();
    mResourceNodes.clear();
    mResourceRegistry.clear();
    mAliases.clear();
}

void FrameGraph::export_graphviz(utils::io::ostream& out) {
    bool removeCulled = false;

    out << "digraph framegraph {\n";
    out << "rankdir = LR\n";
    out << "bgcolor = black\n";
    out << "node [shape=rectangle, fontname=\"helvetica\", fontsize=10]\n\n";

    auto const& registry = mResourceNodes;
    auto const& frameGraphPasses = mPassNodes;

    // declare passes
    for (auto const& node : frameGraphPasses) {
        if (removeCulled && !node.refCount) continue;
        out << "\"P" << node.id << "\" [label=\"" << node.name
               << "\\nrefs: " << node.refCount
               << "\\nseq: " << node.id
               << "\", style=filled, fillcolor="
               << (node.refCount ? "darkorange" : "darkorange4") << "]\n";
    }

    // declare resources nodes
    out << "\n";
    for (auto const& node : registry) {
        auto subresource = registry[node.index].resource;
        if (removeCulled && !subresource->readerCount) continue;
        for (size_t version = 0; version <= node.version; version++) {
            out << "\"R" << node.index << "_" << version << "\""
                   "[label=\"" << node.name << "\\n(version: " << version << ")"
                   "\\nid:" << node.index <<
                   "\\nrefs:" << subresource->readerCount
                   <<"\""
                   ", style=filled, fillcolor="
                   << (subresource->readerCount ? "skyblue" : "skyblue4") << "]\n";
        }
    }

    // connect passes to resources
    out << "\n";
    for (auto const& node : frameGraphPasses) {
        if (removeCulled && !node.refCount) continue;
        out << "P" << node.id << " -> { ";
        for (auto const& writer : node.writes) {
            auto resource = registry[writer.index].resource;
            if (removeCulled && !resource->readerCount) continue;
            out << "R" << writer.index << "_" << writer.version << " ";
        }
        out << "} [color=red2]\n";
    }

    // connect resources to passes
    out << "\n";
    for (auto const& node : registry) {
        auto subresource = registry[node.index].resource;
        if (removeCulled && !subresource->readerCount) continue;
        for (size_t version = 0; version <= node.version; version++) {
            out << "R" << node.index << "_" << version << " -> { ";

            // who reads us...
            for (auto const& pass : frameGraphPasses) {
                if (removeCulled && !pass.refCount) continue;
                for (auto const& read : pass.reads) {
                    if (read.index == node.index && read.version == version ) {
                        out << "P" << pass.id << " ";
                    }
                }
            }
            out << "} [color=lightgreen]\n";
        }
    }

    // aliases...
    if (!mAliases.empty()) {
        out << "\n";
        for (auto const& alias : mAliases) {
            out << "R" << alias.from.index << "_" << alias.from.version << " -> ";
            out << "R" << alias.to.index << "_" << alias.to.version;
            out << " [color=yellow, style=dashed]\n";
        }
    }

    out << "}" << utils::io::endl;
}

} // namespace filament
