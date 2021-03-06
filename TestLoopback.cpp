// Copyright (c) 2016-2016 Lime Microsystems
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <Poco/JSON/Object.h>
#include <iostream>
#include "LoRaCodes.hpp"

POTHOS_TEST_BLOCK("/lora/tests", test_encoder_to_decoder)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto registry = env->findProxy("Pothos/BlockRegistry");

    auto feeder = registry.callProxy("/blocks/feeder_source", "uint8");
    auto encoder = registry.callProxy("/lora/lora_encoder");
    auto decoder = registry.callProxy("/lora/lora_decoder");
    auto collector = registry.callProxy("/blocks/collector_sink", "uint8");

    std::vector<std::string> testCodingRates;
    testCodingRates.push_back("4/4");
    testCodingRates.push_back("4/5");
    testCodingRates.push_back("4/6");
    testCodingRates.push_back("4/7");
    testCodingRates.push_back("4/8");

    for (size_t SF = 7; SF <= 12; SF++)
    {
        std::cout << "Testing SF " << SF << std::endl;
        for (const auto &CR : testCodingRates)
        {
            std::cout << "  with CR " << CR << std::endl;
            encoder.callVoid("setSpreadFactor", SF);
            decoder.callVoid("setSpreadFactor", SF);
            encoder.callVoid("setCodingRate", CR);
            decoder.callVoid("setCodingRate", CR);

            //create a test plan
            Poco::JSON::Object::Ptr testPlan(new Poco::JSON::Object());
            testPlan->set("enablePackets", true);
            testPlan->set("minValue", 0);
            testPlan->set("maxValue", 255);
            auto expected = feeder.callProxy("feedTestPlan", testPlan);

            //create tester topology
            {
                Pothos::Topology topology;
                topology.connect(feeder, 0, encoder, 0);
                topology.connect(encoder, 0, decoder, 0);
                topology.connect(decoder, 0, collector, 0);
                topology.commit();
                POTHOS_TEST_TRUE(topology.waitInactive());
                //std::cout << topology.queryJSONStats() << std::endl;
            }

            std::cout << "verifyTestPlan" << std::endl;
            collector.callVoid("verifyTestPlan", expected);
        }
    }
}

POTHOS_TEST_BLOCK("/lora/tests", test_loopback)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto registry = env->findProxy("Pothos/BlockRegistry");

    const size_t SF = 10;
    auto feeder = registry.callProxy("/blocks/feeder_source", "uint8");
    auto encoder = registry.callProxy("/lora/lora_encoder");
    auto mod = registry.callProxy("/lora/lora_mod", SF);
    auto adder = registry.callProxy("/comms/arithmetic", "complex_float32", "ADD");
    auto noise = registry.callProxy("/comms/noise_source", "complex_float32");
    auto demod = registry.callProxy("/lora/lora_demod", SF);
    auto decoder = registry.callProxy("/lora/lora_decoder");
    auto collector = registry.callProxy("/blocks/collector_sink", "uint8");

    std::vector<std::string> testCodingRates;
    //these first few dont have error correction
    //testCodingRates.push_back("4/4");
    //testCodingRates.push_back("4/5");
    //testCodingRates.push_back("4/6");
    testCodingRates.push_back("4/7");
    testCodingRates.push_back("4/8");

    for (const auto &CR : testCodingRates)
    {
        std::cout << "Testing with CR " << CR << std::endl;

        encoder.callVoid("setSpreadFactor", SF);
        decoder.callVoid("setSpreadFactor", SF);
        encoder.callVoid("setCodingRate", CR);
        decoder.callVoid("setCodingRate", CR);
        mod.callVoid("setAmplitude", 1.0);
        noise.callVoid("setAmplitude", 4.0);
        noise.callVoid("setWaveform", "NORMAL");
        mod.callVoid("setPadding", 512);
        demod.callVoid("setMTU", 512);

        //create a test plan
        Poco::JSON::Object::Ptr testPlan(new Poco::JSON::Object());
        testPlan->set("enablePackets", true);
        testPlan->set("minValue", 0);
        testPlan->set("maxValue", 255);
        testPlan->set("minBuffers", 5);
        testPlan->set("maxBuffers", 5);
        testPlan->set("minBufferSize", 8);
        testPlan->set("maxBufferSize", 128);
        auto expected = feeder.callProxy("feedTestPlan", testPlan);

        //create tester topology
        {
            Pothos::Topology topology;
            topology.connect(feeder, 0, encoder, 0);
            topology.connect(encoder, 0, mod, 0);
            topology.connect(mod, 0, adder, 0);
            topology.connect(noise, 0, adder, 1);
            topology.connect(adder, 0, demod, 0);
            topology.connect(demod, 0, decoder, 0);
            topology.connect(decoder, 0, collector, 0);
            topology.commit();
            POTHOS_TEST_TRUE(topology.waitInactive(0.1, 0));
            //std::cout << topology.queryJSONStats() << std::endl;
        }

        std::cout << "decoder dropped " << decoder.call<unsigned long long>("getDropped") << std::endl;
        std::cout << "verifyTestPlan" << std::endl;
        collector.callVoid("verifyTestPlan", expected);
    }
}
