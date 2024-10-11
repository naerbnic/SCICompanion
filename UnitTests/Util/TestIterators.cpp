
#include "stdafx.h"
#include "Iterators.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    TEST_CLASS(TestIterators)
    {
    public:

        class TestIteratorImpl : public IteratorImpl<int>
        {
        public:
            TestIteratorImpl(int start, int end) : current_(start), end_(end) {}

            bool AtEnd() const override
            {
                return current_ >= end_;
            }

            void Next() override
            {
                current_++;
            }

            int GetCurrent() const override
            {
                return current_;
            }
        private:
            int current_;
            int end_;
        };

        class TestContainerImpl : public ContainerImpl<int>
        {
        public:
            TestContainerImpl(int start, int end) : start_(start), end_(end) {}

            std::unique_ptr<IteratorImpl<int>> CreateIterator() override
            {
                return std::make_unique<TestIteratorImpl>(start_, end_);
            }

        private:
            int start_;
            int end_;
        };

        TEST_METHOD(BasicIteratorWorks)
        {
            auto container = Container<int>(std::make_shared<TestContainerImpl>(0, 10));

            std::vector<int> expectedValues = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
            std::vector<int> actualValues;
            for (auto i : container)
            {
                actualValues.push_back(i);
            }
            Assert::IsTrue(expectedValues == actualValues);
        }
    };
}