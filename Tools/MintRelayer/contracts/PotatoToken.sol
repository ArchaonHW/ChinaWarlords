// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// Story 11.5 — the mint currency. Minimal ERC-20, decimals 0:
// the game's discipline is integer-only, and the token keeps it.
// mint() is onlyOwner (the relayer/dev wallet) and dedupes on
// the claim id — a re-submitted claim returns false and mints
// nothing, so the batch can be safely replayed.
contract PotatoToken {
    string public constant name = "Potato Chronicle";
    string public constant symbol = "POT";
    uint8 public constant decimals = 0;

    uint256 public totalSupply;
    mapping(address => uint256) public balanceOf;
    // claimId = keccak256(utf8 bytes of the potato.mintclaim/1 id)
    mapping(bytes32 => bool) public claimed;
    address public owner;

    event Transfer(address indexed from, address indexed to,
                   uint256 value);
    event Minted(bytes32 indexed claimId, address indexed to,
                 uint256 amount);

    error NotOwner();

    constructor() {
        owner = msg.sender;
    }

    function mint(bytes32 claimId, address to, uint256 amount)
        external returns (bool) {
        if (msg.sender != owner) revert NotOwner();
        if (claimed[claimId]) return false; // idempotent dedupe
        claimed[claimId] = true;
        totalSupply += amount;
        balanceOf[to] += amount;
        emit Transfer(address(0), to, amount);
        emit Minted(claimId, to, amount);
        return true;
    }

    // Dev-chain burn: the relayer (owner) debits the player as
    // payment for a strike. Production would use the standard
    // approve/burnFrom allowance path — deferred-work noted.
    function burnFrom(address from, uint256 amount)
        external returns (bool) {
        if (msg.sender != owner) revert NotOwner();
        require(balanceOf[from] >= amount, "insufficient");
        balanceOf[from] -= amount;
        totalSupply -= amount;
        emit Transfer(from, address(0), amount);
        return true;
    }

    function transfer(address to, uint256 amount)
        external returns (bool) {
        require(balanceOf[msg.sender] >= amount, "insufficient");
        balanceOf[msg.sender] -= amount;
        balanceOf[to] += amount;
        emit Transfer(msg.sender, to, amount);
        return true;
    }
}
