/* Copyright (C) 2013 BMW Group
 * Author: Manfred Bathelt (manfred.bathelt@bmw.de)
 * Author: Juergen Gehring (juergen.gehring@bmw.de)
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Benchmark.h"

#include <CommonAPI/Factory.h>
#include <functional>


static const std::string serviceAddress = "local:comommonapi.tests.EchoService:commonapi.tests.Echo";


namespace CommonAPI {
namespace tests {

Benchmark::Benchmark(SendType sendType, unsigned long sendCount, unsigned long arraySize, bool verbose, bool async):
		sendType_(sendType),
		sendCount_(sendCount),
		arraySize_(arraySize),
		verbose_(verbose),
		async_(async),
		benchmarkStats_("GENIVI_ECHO", sendCount_, verbose_),
		getEmptyResponseAsyncCallback_(std::bind(&Benchmark::emptyAsyncSendBenchmarkCallback, this, std::placeholders::_1)),
		getTestDataCopyAsyncCallback_(std::bind(&Benchmark::copyAsyncSendBenchmarkCallback, this, std::placeholders::_1, std::placeholders::_2)),
		getTestDataArrayCopyAsyncCallback_(std::bind(&Benchmark::copiesAsyncSendBenchmarkCallback, this, std::placeholders::_1, std::placeholders::_2)) {
}

bool Benchmark::run() {
	std::shared_ptr<CommonAPI::Factory> factory = CommonAPI::Runtime::load()->createFactory();

	echoProxy_ = factory->buildProxy<CommonAPI::tests::EchoProxy>(serviceAddress);

	std::cout << "Waiting for EchoService: " << serviceAddress << std::endl;
	for (int i = 0; !echoProxy_->isAvailable() && i < 10; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	if (!echoProxy_->isAvailable()) {
		std::cerr << "EchoService is not available: timed out waiting!\n";
		return false;
	}

	if (verbose_) {
		std::cout << "Benchmark begin: type=" << sendType_ << ", count=" << sendCount_ << std::endl;
	}

	benchmarkStats_.reset();

	CommonAPI::CallStatus result = CommonAPI::CallStatus::NOT_AVAILABLE;

	switch (sendType_) {
	case SEND_TYPE_EMPTY:
		result = async_ ? doEmptyAsyncSendBenchmark() : doEmptySendBenchmark();
		break;

	case SEND_TYPE_COPY:
		result = async_ ? doCopyAsyncSendBenchmark() : doCopySendBenchmark();
		break;

	case SEND_TYPE_COPIES:
		result = async_ ? doCopiesAsyncSendBenchmark() : doCopiesSendBenchmark();
		break;

	default:
		std::cerr << "Benchmark type=" << sendType_ << " not implemented!\n";
		break;
	}

	if (verbose_) {
		std::cout << "Benchmark end: type=" << sendType_ << ", count=" << sendCount_ << std::endl;
	}

	benchmarkStats_.stop();

	echoProxy_.reset();

	return result == CommonAPI::CallStatus::SUCCESS;
}

CommonAPI::tests::Echo::TestData Benchmark::createTestData() {
	if (verbose_) {
		std::cout << "Creating TestData struct...\n";
	}

	benchmarkStats_.startCreation();

    Echo::TestData testData(1, 12.6, 1e40, "XXXXXXXXXXXXXXXXXXXX");

    benchmarkStats_.stopCreation();

    return testData;
}

CommonAPI::tests::Echo::TestDataArray Benchmark::createTestDataArray() {
	if (verbose_) {
		std::cout << "Creating TestDataArray of size=" << arraySize_ << "...\n";
	}

    benchmarkStats_.startCreation();

    CommonAPI::tests::Echo::TestDataArray testDataArray;

    for (unsigned long i = 0; i < arraySize_; i++)
    	testDataArray.emplace_back(1, 12.6, 1e40, "XXXXXXXXXXXXXXXXXXXX");

    benchmarkStats_.stopCreation();

    return testDataArray;
}

CommonAPI::CallStatus Benchmark::doEmptySendBenchmark() {
	CallStatus callStatus;

	for (unsigned long i = 0; i < sendCount_; i++) {
    	benchmarkStats_.startTransport();
        echoProxy_->getEmptyResponse(callStatus);

        if (callStatus != CallStatus::SUCCESS) {
        	break;
        }
        benchmarkStats_.stopTransport();

        benchmarkStats_.addSendReplyDelta();
    }

    return callStatus;
}

CommonAPI::CallStatus Benchmark::doCopySendBenchmark() {
	CallStatus callStatus;
	Echo::TestData testData = createTestData();
	Echo::TestData testDataReply;

    for (unsigned long i = 0; i < sendCount_; i++) {
    	benchmarkStats_.startTransport();
    	echoProxy_->getTestDataCopy(testData, callStatus, testDataReply);

        if (callStatus != CallStatus::SUCCESS) {
        	break;
        }
        benchmarkStats_.stopTransport();


        benchmarkStats_.startCreation();
        testData = testDataReply;
        benchmarkStats_.stopCreation();

        benchmarkStats_.addSendReplyDelta();
    }

    return callStatus;
}

CommonAPI::CallStatus Benchmark::doCopiesSendBenchmark() {
	CallStatus callStatus;
	Echo::TestDataArray testDataArray = createTestDataArray();
	Echo::TestDataArray testDataArrayReply;

	for (unsigned long i = 0; i < sendCount_; i++) {
    	benchmarkStats_.startTransport();
    	echoProxy_->getTestDataArrayCopy(testDataArray, callStatus, testDataArrayReply);

        if (callStatus != CallStatus::SUCCESS) {
        	break;
        }
        benchmarkStats_.stopTransport();

        benchmarkStats_.startCreation();
        testDataArray = testDataArrayReply;
        benchmarkStats_.stopCreation();

        benchmarkStats_.addSendReplyDelta();
    }

    return callStatus;
}

CommonAPI::CallStatus Benchmark::doEmptyAsyncSendBenchmark() {
    benchmarkStats_.startTransport();
    echoProxy_->getEmptyResponseAsync(getEmptyResponseAsyncCallback_);

    auto asyncFuture = asyncPromise_.get_future();
    const CommonAPI::CallStatus& callStatus = asyncFuture.get();

    return callStatus;
}

void Benchmark::emptyAsyncSendBenchmarkCallback(const CommonAPI::CallStatus& callStatus)  {
	benchmarkStats_.stopTransport();
    benchmarkStats_.addSendReplyDelta();

    if (callStatus == CallStatus::SUCCESS && benchmarkStats_.getSendCount() < sendCount_) {
    	benchmarkStats_.startTransport();
    	echoProxy_->getEmptyResponseAsync(getEmptyResponseAsyncCallback_);
    } else {
    	asyncPromise_.set_value(callStatus);
    }
}

CommonAPI::CallStatus Benchmark::doCopyAsyncSendBenchmark() {
	Echo::TestData testData = createTestData();

	benchmarkStats_.startTransport();
    echoProxy_->getTestDataCopyAsync(testData, getTestDataCopyAsyncCallback_);

    auto asyncFuture = asyncPromise_.get_future();
    const CommonAPI::CallStatus& callStatus = asyncFuture.get();

    return callStatus;
}

void Benchmark::copyAsyncSendBenchmarkCallback(const CommonAPI::CallStatus& callStatus, const Echo::TestData& testDataReply)  {
	benchmarkStats_.stopTransport();
    benchmarkStats_.addSendReplyDelta();

    benchmarkStats_.startCreation();
    Echo::TestData testData = testDataReply;
    benchmarkStats_.stopCreation();

    if (callStatus == CallStatus::SUCCESS && benchmarkStats_.getSendCount() < sendCount_) {
    	benchmarkStats_.startTransport();
    	echoProxy_->getTestDataCopyAsync(testData, getTestDataCopyAsyncCallback_);
    } else {
    	asyncPromise_.set_value(callStatus);
    }
}

CommonAPI::CallStatus Benchmark::doCopiesAsyncSendBenchmark() {
	Echo::TestDataArray testDataArray = createTestDataArray();

	benchmarkStats_.startTransport();
    echoProxy_->getTestDataArrayCopyAsync(testDataArray, getTestDataArrayCopyAsyncCallback_);

    auto asyncFuture = asyncPromise_.get_future();
    const CommonAPI::CallStatus& callStatus = asyncFuture.get();

    return callStatus;
}

void Benchmark::copiesAsyncSendBenchmarkCallback(const CommonAPI::CallStatus& callStatus, const Echo::TestDataArray& testDataArrayReply)  {
	benchmarkStats_.stopTransport();
    benchmarkStats_.addSendReplyDelta();

    benchmarkStats_.startCreation();
    Echo::TestDataArray testDataArray = testDataArrayReply;
    benchmarkStats_.stopCreation();

    if (callStatus == CallStatus::SUCCESS && benchmarkStats_.getSendCount() < sendCount_) {
    	benchmarkStats_.startTransport();
    	echoProxy_->getTestDataArrayCopyAsync(testDataArray, getTestDataArrayCopyAsyncCallback_);
    } else {
    	asyncPromise_.set_value(callStatus);
    }
}

} // namespace tests
} // namespace CommonAPI
