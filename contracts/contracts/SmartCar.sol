pragma solidity ^0.4.24;

contract SmartCar {
    address public owner;

    // states of car leasing
    enum State {active, expiring, awaitingPayment}

    // timestamp until lease runs out
    uint256 private paidUntil;
    // price lease per second
    uint256 public pricePerSecond;
    // after lease runs out a warning period begins
    // period in seconds
    uint256 private warningPeriod;
    
    // payment event 
    event Payment(address indexed payer, uint256 amount);
    
    // owner modifier
    modifier onlyOwner() {
        require(owner == msg.sender);
        _;
    }
    
    constructor(uint256 _pricePerSecond, uint256 _warningPeriod) public {
        owner = msg.sender;
        pricePerSecond = _pricePerSecond;
        warningPeriod = _warningPeriod;
    }
    
    // fallback function
    function () public payable {
        require(msg.value > 0);
        // calculate bought time and remainder
        uint256 timeBought = msg.value / pricePerSecond;
        uint256 remainder = msg.value % pricePerSecond;
        // if car is currently not leased, add bought time to current timestamp
        if (paidUntil < now) {
            paidUntil = timeBought + now;
        // if car is currently leased, add bought time to paidUntil
        } else {
            paidUntil += timeBought;
        }
        // refund remainder
        msg.sender.transfer(remainder);
        emit Payment(msg.sender, msg.value - remainder);
    }
    
    // only owner can call payout function
    function payout() public onlyOwner {
        owner.transfer(address(this).balance);
    }
    
    // return current leasing state
    function getLeasingState() public view returns(State) {
        // if current timestamp is greater than end of lease + warningPeriod
        // return awaitingPayment
        if (now > endOfWarningTimestamp()) {
            return State.awaitingPayment;
        // if current timestamp is greater than end of lease and less than end of lease + warningPeriod
        // return expiring
        } else if (now > expirationTimestamp()) {
            return State.expiring;
        // if current timestamp is less than paidUntil
        // return active
        } else {
            return State.active;
        }
    }
    
    // get current timestamp
    function currentTimestamp() public view returns(uint256) {
        return now;
    }
    
    // get expiration timestamp of lease
    function expirationTimestamp() public view returns(uint256) {
        return paidUntil;
    }
    
    // get expiration timestamp of lease and warning period
    function endOfWarningTimestamp() public view returns(uint256) {
        return paidUntil + warningPeriod;
    }
    
    
}