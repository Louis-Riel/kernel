exports.fromVersionedToPlain=(obj)=>{
    let ret = isObject(obj) ? {} : [];
    if (isObject(obj)){
        Object.entries(obj).forEach(fld => {
            if (isObject(fld[1])) {
                ret[fld[0]] = (fld[1].value !== undefined) && (fld[1].version !== undefined) ? fld[1].value 
                                                                                             : this.fromVersionedToPlain(fld[1]);
            } else if (isArray(fld[1])) {
                ret[fld[0]] = fld[1].map(this.fromVersionedToPlain.bind(this));
            } else {
                ret[fld[0]] = fld[1];
            }
        })
    }
    return ret;
};

function isArray(a) {
    return (!!a) && (a.constructor === Array);
}

function isObject(a) {
    return (!!a) && (a.constructor === Object);
}
