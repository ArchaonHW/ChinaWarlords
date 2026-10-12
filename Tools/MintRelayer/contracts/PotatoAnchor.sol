// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// Story 11.5 — the chronicle's on-chain seal. One mapping of
// merkle-root → first-seen timestamp; anchoring the same root
// twice is a no-op returning the original stamp (on-chain
// idempotence — the relayer's resubmit guarantee lives here,
// not in client-side bookkeeping).
contract PotatoAnchor {
    mapping(bytes32 => uint64) private _anchored;

    event Anchored(bytes32 indexed root, uint64 at);

    function anchor(bytes32 root) external returns (uint64 at) {
        at = _anchored[root];
        if (at == 0) {
            at = uint64(block.timestamp);
            _anchored[root] = at;
            emit Anchored(root, at);
        }
    }

    function anchoredAt(bytes32 root) external view returns (uint64) {
        return _anchored[root];
    }
}
