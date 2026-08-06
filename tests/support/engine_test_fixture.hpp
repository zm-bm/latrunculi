#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "board/board.hpp"
#include "search/search_thread_pool.hpp"
#include "search/tt.hpp"
#include "uci/engine.hpp"
#include "gtest/gtest.h"

class EngineTest : public ::testing::Test {
protected:
    std::ostringstream output;
    uci::Engine        engine{output, output, std::cin};

    void SetUp() override {
        output.str("");
        output.clear();
        tt.clear();
    }

    bool                execute(const std::string& command) { return engine.execute(command); }
    Board&              board() { return engine.board; }
    SearchThreadPool&   thread_pool() { return engine.thread_pool; }
    const uci::Options& options() const { return engine.options; }
};
