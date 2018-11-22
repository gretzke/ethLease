var SmartCar = artifacts.require("./SmartCar.sol");

module.exports = function (deployer) {
    // deploy smart contract
    // set price per second to 30 gwei
    // set warning period to 30 seconds
    deployer.deploy(SmartCar, web3.toWei(1, "gwei"), 30);
};