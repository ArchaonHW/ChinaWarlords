// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// Story 11.6 — the keepsake plates (藏書票). Minimal ERC-721-
// shaped collectible: each strike stores its potato.collectible/1
// descriptor verbatim on-chain (dev chain: gas is free; a
// production variant would tokenURI to IPFS — not built here).
// strike() is onlyOwner + struck[key] dedupe: a player striking
// the same artifact twice is a no-op, same contract-level
// idempotence discipline as the token's claimed[] map.
contract PotatoCollectible {
    string public constant name = "Potato Keepsake";
    string public constant symbol = "KPSK";

    uint256 public totalMinted;
    mapping(uint256 => address) public ownerOf;
    mapping(address => uint256) public balanceOf;
    mapping(uint256 => string) public descriptor;
    mapping(bytes32 => bool) public struck;
    address public owner;

    event Struck(uint256 indexed tokenId, bytes32 indexed key,
                 address indexed to);

    error NotOwner();

    constructor() {
        owner = msg.sender;
    }

    // key = keccak256(utf8 "<kind>:<source_id>") — computed by the
    // relayer off-chain so the dedupe space matches the artifact.
    function strike(bytes32 key, address to, string calldata desc)
        external returns (uint256 tokenId) {
        if (msg.sender != owner) revert NotOwner();
        if (struck[key]) return 0; // already struck — no-op
        struck[key] = true;
        tokenId = ++totalMinted;
        ownerOf[tokenId] = to;
        balanceOf[to] += 1;
        descriptor[tokenId] = desc;
        emit Struck(tokenId, key, to);
    }
}
