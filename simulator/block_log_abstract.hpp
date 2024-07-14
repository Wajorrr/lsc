#pragma once

#include <vector>
#include "block.hpp"

namespace flashCache
{

    class BlockLogAbstract
    {
    public:
        /* ----------- Basic functionality --------------- */
        virtual ~BlockLogAbstract() = default;

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        virtual std::vector<Block> insert(std::vector<Block> items) = 0;

        /* returns true if the item is in sets layer */
        virtual bool find(Block item) = 0;

        /* ----------- Useful for Admission Policies --------------- */

        /* for readmission policies, call on anything dropped before sets */
        virtual void readmit(std::vector<Block> items) = 0;

        /* returns ratio of total capacity used */
        virtual double ratioCapacityUsed() = 0;

        /* ----------- Other Bookeeping --------------- */

        virtual double calcWriteAmp() = 0;
        virtual void flushStats() = 0;
    };

} // namespace flashCache
