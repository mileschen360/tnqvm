/***********************************************************************************
 * Copyright (c) 2017, UT-Battelle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the xacc nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contributors:
 *   Initial API and implementation - Alex McCaskey
 *
 **********************************************************************************/
#include "TNQVM.hpp"
#include "ServiceRegistry.hpp"
#ifdef TNQVM_HAS_EXATENSOR
#include "ExaTensorMPSVisitor.hpp"
#endif

namespace tnqvm {

std::shared_ptr<AcceleratorBuffer> TNQVM::createBuffer(
		const std::string& varId) {
	return createBuffer(varId, 100);
}

std::shared_ptr<AcceleratorBuffer> TNQVM::createBuffer(
		const std::string& varId, const int size) {
	if (!isValidBufferSize(size)) {
		XACCError("Invalid buffer size.");
	}
	auto derived_buffer = std::make_shared<TNQVMBuffer>(varId, size);
	derived_buffer->set_verbose(__verbose);
	std::shared_ptr<AcceleratorBuffer> buffer = derived_buffer;
	storeBuffer(varId, buffer);
	return buffer;
}

bool TNQVM::isValidBufferSize(const int NBits) {
	return NBits <= 1000;
}

std::vector<std::shared_ptr<AcceleratorBuffer>> TNQVM::execute(
		std::shared_ptr<AcceleratorBuffer> buffer,
		const std::vector<std::shared_ptr<Function>> functions) {
	int counter = 0;
	std::vector<std::shared_ptr<AcceleratorBuffer>> tmpBuffers;
	for (auto f : functions) {
		auto tmpBuffer = createBuffer(
				buffer->name() + std::to_string(counter), buffer->size());
		execute(tmpBuffer, f);
		tmpBuffers.push_back(tmpBuffer);
		counter++;
	}

	return tmpBuffers;
}

void TNQVM::execute(std::shared_ptr<AcceleratorBuffer> buffer,
		const std::shared_ptr<xacc::Function> kernel) {

	if (!std::dynamic_pointer_cast<TNQVMBuffer>(buffer)) {
		XACCError("Invalid AcceleratorBuffer, must be a TNQVMBuffer.");
	}

	auto options = RuntimeOptions::instance();
	std::string visitorType = "itensor-mps";
	if (options->exists("tnqvm-visitor")) {
		visitorType = (*options)["tnqvm-visitor"];
	}

	// Get the visitor backend
	visitor = ServiceRegistry::instance()->getService<TNQVMVisitor>(
			visitorType);

	// Initialize the visitor
	visitor->initialize(std::dynamic_pointer_cast<TNQVMBuffer>(buffer));

	// Cast to a base instruction visitor for the accept call
	auto visCast =
			std::dynamic_pointer_cast<BaseInstructionVisitor>(visitor);

	// Walk the IR tree, and visit each node
	InstructionIterator it(kernel);
	while (it.hasNext()) {
		auto nextInst = it.next();
		if (nextInst->isEnabled()) {
			nextInst->accept(
					visCast);
		}
	}

	// Finalize the visitor
	visitor->finalize();

}

}
