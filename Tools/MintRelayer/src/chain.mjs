// Contract compile + deploy helpers — shared by the deploy
// script and the ganache test suite. solc/ethers load lazily so
// the zero-dep validator core never drags them in.
import { readFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const CONTRACTS = join(
  dirname(fileURLToPath(import.meta.url)), "..", "contracts");

export async function compileContracts() {
  const solc = (await import("solc")).default;
  const sources = {};
  for (const name of ["PotatoAnchor.sol", "PotatoToken.sol",
                      "PotatoCollectible.sol"]) {
    sources[name] = {
      content: await readFile(join(CONTRACTS, name), "utf8"),
    };
  }
  const out = JSON.parse(solc.compile(JSON.stringify({
    language: "Solidity",
    sources,
    settings: {
      // solc >=0.8.25 defaults to cancun (MCOPY/TSTORE) — ganache's
      // EVM tops out at shanghai; pin it so dynamic-string getters
      // don't compile to unsupported opcodes.
      evmVersion: "shanghai",
      outputSelection: {
        "*": { "*": ["abi", "evm.bytecode.object"] },
      },
    },
  })));
  if (out.errors?.some((e) => e.severity === "error")) {
    const msg = out.errors
      .filter((e) => e.severity === "error")
      .map((e) => e.formattedMessage).join("\n");
    throw new Error(`solc: ${msg}`);
  }
  const pick = (file, name) => ({
    abi: out.contracts[file][name].abi,
    bytecode: "0x" + out.contracts[file][name].evm.bytecode.object,
  });
  return {
    PotatoAnchor: pick("PotatoAnchor.sol", "PotatoAnchor"),
    PotatoToken: pick("PotatoToken.sol", "PotatoToken"),
    PotatoCollectible: pick("PotatoCollectible.sol",
                            "PotatoCollectible"),
  };
}

// ethers Signer → {anchor, token, collectible} addresses + ABIs.
export async function deploy(signer) {
  const { ContractFactory } = await import("ethers");
  const c = await compileContracts();
  const anchor = await (await new ContractFactory(
    c.PotatoAnchor.abi, c.PotatoAnchor.bytecode, signer)
    .deploy()).waitForDeployment();
  const token = await (await new ContractFactory(
    c.PotatoToken.abi, c.PotatoToken.bytecode, signer)
    .deploy()).waitForDeployment();
  const collectible = await (await new ContractFactory(
    c.PotatoCollectible.abi, c.PotatoCollectible.bytecode, signer)
    .deploy()).waitForDeployment();
  return {
    anchor: await anchor.getAddress(),
    token: await token.getAddress(),
    collectible: await collectible.getAddress(),
    anchorAbi: c.PotatoAnchor.abi,
    tokenAbi: c.PotatoToken.abi,
    collectibleAbi: c.PotatoCollectible.abi,
  };
}
